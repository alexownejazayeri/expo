#include "JSIUtils.h"
#include "EventEmitter.h"

namespace expo::EventEmitter {

#pragma mark - Listeners

void Listeners::add(jsi::Runtime &runtime, std::string eventName, const jsi::Function &listener) noexcept {
  listenersMap[eventName].emplace_back(runtime, listener);
}

void Listeners::remove(jsi::Runtime &runtime, std::string eventName, const jsi::Function &listener) noexcept {
  if (!listenersMap.contains(eventName)) {
    return;
  }
  jsi::Value listenerValue(runtime, listener);

  listenersMap[eventName].remove_if([&](const jsi::Value &item) {
    return jsi::Value::strictEquals(runtime, listenerValue, item);
  });
}

void Listeners::removeAll(std::string eventName) noexcept {
  if (listenersMap.contains(eventName)) {
    listenersMap[eventName].clear();
  }
}

void Listeners::clear() noexcept {
  listenersMap.clear();
}

size_t Listeners::listenersCount(std::string eventName) noexcept {
  if (!listenersMap.contains(eventName)) {
    return 0;
  }
  return listenersMap[eventName].size();
}

void Listeners::call(jsi::Runtime &runtime, std::string eventName, const jsi::Object &thisObject, const jsi::Value *args, size_t count) noexcept {
  if (!listenersMap.contains(eventName)) {
    return;
  }
  ListenersList &listenersList = listenersMap[eventName];

  for (const jsi::Value &listener : listenersList) {
    listener
      .asObject(runtime)
      .asFunction(runtime)
      .callWithThis(runtime, thisObject, args, count);
  }
}

#pragma mark - NativeState

NativeState::NativeState() : jsi::NativeState() {}

NativeState::~NativeState() {
  listeners.clear();
}

NativeState::Shared NativeState::get(jsi::Runtime &runtime, jsi::Object &object, bool createIfMissing) {
  if (object.hasNativeState<NativeState>(runtime)) {
    return object.getNativeState<NativeState>(runtime);
  }
  if (createIfMissing) {
    NativeState::Shared state = std::make_shared<NativeState>();
    object.setNativeState(runtime, state);
    return state;
  }
  return nullptr;
}

#pragma mark - SubscriptionNativeState

SubscriptionNativeState::SubscriptionNativeState(jsi::Object emitter, jsi::Function listener)
  : jsi::NativeState(), emitter(std::move(emitter)), listener(std::move(listener)) {}

#pragma mark - Utils

void callObservingFunction(jsi::Runtime &runtime, jsi::Object &object, const char* functionName, std::string eventName) {
  jsi::Value fnValue = object.getProperty(runtime, functionName);

  if (!fnValue.isObject()) {
    // Skip it if there is no observing function.
    return;
  }

  fnValue
    .getObject(runtime)
    .asFunction(runtime)
    .callWithThis(runtime, object, {
      jsi::Value(runtime, jsi::String::createFromUtf8(runtime, eventName))
    });
}

void addListener(jsi::Runtime &runtime, jsi::Object &emitter, const std::string &eventName, const jsi::Function &listener) {
  if (NativeState::Shared state = NativeState::get(runtime, emitter, true)) {
    state->listeners.add(runtime, eventName, listener);

    if (state->listeners.listenersCount(eventName) == 1) {
      callObservingFunction(runtime, emitter, "startObserving", eventName);
    }
  }
}

void removeListener(jsi::Runtime &runtime, jsi::Object &emitter, const std::string &eventName, const jsi::Function &listener) {
  if (NativeState::Shared state = NativeState::get(runtime, emitter, false)) {
    size_t listenersCountBefore = state->listeners.listenersCount(eventName);

    state->listeners.remove(runtime, eventName, listener);

    if (listenersCountBefore >= 1 && state->listeners.listenersCount(eventName) == 0) {
      callObservingFunction(runtime, emitter, "stopObserving", eventName);
    }
  }
}

void removeAllListeners(jsi::Runtime &runtime, jsi::Object &emitter, const std::string &eventName) {
  if (NativeState::Shared state = NativeState::get(runtime, emitter, false)) {
    size_t listenersCountBefore = state->listeners.listenersCount(eventName);

    state->listeners.removeAll(eventName);

    if (listenersCountBefore >= 1) {
      callObservingFunction(runtime, emitter, "stopObserving", eventName);
    }
  }
}

void emitEvent(jsi::Runtime &runtime, jsi::Object &emitter, const std::string &eventName, const jsi::Value *args, size_t count) {
  if (NativeState::Shared state = NativeState::get(runtime, emitter, false)) {
    // Pass the arguments without the first that is the event name.
    const jsi::Value *payloadArgs = count > 1 ? &args[1] : nullptr;
    state->listeners.call(runtime, eventName, emitter, payloadArgs, count - 1);
  }
}

jsi::Object createEventSubscription(jsi::Runtime &runtime, const std::string &eventName, jsi::Object emitter, jsi::Function listener) {
  jsi::Object subscription(runtime);
  jsi::PropNameID removeProp = jsi::PropNameID::forAscii(runtime, "remove", 6);
  SubscriptionNativeState::Shared nativeState = std::make_shared<SubscriptionNativeState>(std::move(emitter), std::move(listener));
  jsi::HostFunctionType removeSubscription = [eventName](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value {
    jsi::Object thisObject = thisValue.getObject(runtime);

    if (SubscriptionNativeState::Shared state = thisObject.getNativeState<SubscriptionNativeState>(runtime)) {
      removeListener(runtime, state->emitter, eventName, state->listener);
    }
    return jsi::Value::undefined();
  };

  subscription.setNativeState(runtime, nativeState);
  subscription.setProperty(runtime, removeProp, jsi::Function::createFromHostFunction(runtime, removeProp, 0, removeSubscription));

  return subscription;
}

#pragma mark - Public API

jsi::Function getClass(jsi::Runtime &runtime) {
  return common::getCoreObject(runtime)
    .getPropertyAsFunction(runtime, "EventEmitter");
}

void installClass(jsi::Runtime &runtime) {
  jsi::Function eventEmitterClass = common::createClass(runtime, "EventEmitter");
  jsi::Object prototype = eventEmitterClass.getPropertyAsObject(runtime, "prototype");

  jsi::HostFunctionType addListenerHost = [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value {
    std::string eventName = args[0].asString(runtime).utf8(runtime);
    jsi::Function listener = args[1].asObject(runtime).asFunction(runtime);
    jsi::Object thisObject = thisValue.getObject(runtime);

    addListener(runtime, thisObject, eventName, listener);
    return createEventSubscription(runtime, eventName, std::move(thisObject), std::move(listener));
  };

  jsi::HostFunctionType removeListenerHost = [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value {
    std::string eventName = args[0].asString(runtime).utf8(runtime);
    jsi::Function listener = args[1].asObject(runtime).asFunction(runtime);
    jsi::Object thisObject = thisValue.getObject(runtime);

    removeListener(runtime, thisObject, eventName, listener);
    return jsi::Value::undefined();
  };

  jsi::HostFunctionType removeAllListenersHost = [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value {
    std::string eventName = args[0].asString(runtime).utf8(runtime);
    jsi::Object thisObject = thisValue.getObject(runtime);

    removeAllListeners(runtime, thisObject, eventName);
    return jsi::Value::undefined();
  };

  jsi::HostFunctionType emit = [](jsi::Runtime &runtime, const jsi::Value &thisValue, const jsi::Value *args, size_t count) -> jsi::Value {
    std::string eventName = args[0].asString(runtime).utf8(runtime);
    jsi::Object thisObject = thisValue.getObject(runtime);

    emitEvent(runtime, thisObject, eventName, args, count);
    return jsi::Value::undefined();
  };

  jsi::PropNameID addListenerProp = jsi::PropNameID::forAscii(runtime, "addListener", 11);
  jsi::PropNameID removeListenerProp = jsi::PropNameID::forAscii(runtime, "removeListener", 14);
  jsi::PropNameID removeAllListenersProp = jsi::PropNameID::forAscii(runtime, "removeAllListeners", 18);
  jsi::PropNameID emitProp = jsi::PropNameID::forAscii(runtime, "emit", 4);

  prototype.setProperty(runtime, addListenerProp, jsi::Function::createFromHostFunction(runtime, addListenerProp, 2, addListenerHost));
  prototype.setProperty(runtime, removeListenerProp, jsi::Function::createFromHostFunction(runtime, removeListenerProp, 2, removeListenerHost));
  prototype.setProperty(runtime, removeAllListenersProp, jsi::Function::createFromHostFunction(runtime, removeAllListenersProp, 1, removeAllListenersHost));
  prototype.setProperty(runtime, emitProp, jsi::Function::createFromHostFunction(runtime, emitProp, 2, emit));

  common::getCoreObject(runtime)
    .setProperty(runtime, "EventEmitter", eventEmitterClass);
}

} // namespace expo::EventEmitter
