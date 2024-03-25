import { DeviceEventEmitter } from 'react-native';
import { EventEmitter, Subscription } from './EventEmitter';
import NativeModulesProxy from './NativeModulesProxy';
import { ProxyNativeModule } from './NativeModulesProxy.types';
import { requireNativeViewManager } from './NativeViewManagerAdapter';
import Platform from './Platform';
import { CodedError } from './errors/CodedError';
import { UnavailabilityError } from './errors/UnavailabilityError';
import './sweet/setUpErrorManager.fx';
import './web/index';
export type * from './ts-declarations/global';
export type * from './ts-declarations/SharedObject';
export { default as uuid } from './uuid';
export { DeviceEventEmitter, EventEmitter, NativeModulesProxy, ProxyNativeModule, Platform, Subscription, requireNativeViewManager, CodedError, UnavailabilityError, };
export * from './requireNativeModule';
export * from './createWebModule';
export * from './TypedArrays.types';
/**
 * @deprecated renamed to `DeviceEventEmitter`
 */
export declare const SyntheticPlatformEmitter: import("react-native").DeviceEventEmitterStatic;
export * from './PermissionsInterface';
export * from './PermissionsHook';
export * from './Refs';
//# sourceMappingURL=index.d.ts.map