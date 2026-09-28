// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <functional>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace webrtc {
class AudioDeviceModule;
class TaskQueueFactory;
template <class T>
class scoped_refptr;
} // namespace webrtc

namespace rtc {
template <typename T>
using scoped_refptr = webrtc::scoped_refptr<T>;
} // namespace rtc

namespace Webrtc {

struct DeviceResolvedId;

using AudioDeviceModulePtr = rtc::scoped_refptr<webrtc::AudioDeviceModule>;
// Runs on the ADM capture thread, immediately before delivery to WebRTC.
using CaptureProcessor = std::function<void(int16_t*, int, int, int)>;
AudioDeviceModulePtr CreateAudioDeviceModule(
	webrtc::TaskQueueFactory* factory,
	Fn<void(Fn<void(DeviceResolvedId)>)> saveSetDeviceIdCallback,
	CaptureProcessor processor = {});

auto AudioDeviceModuleCreator(
	Fn<void(Fn<void(DeviceResolvedId)>)> saveSetDeviceIdCallback,
	CaptureProcessor processor = {})
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)>;

AudioDeviceModulePtr CreateLoopbackAudioDeviceModule(
	webrtc::TaskQueueFactory* factory);
[[nodiscard]] bool LoopbackAudioCaptureSupported();

auto LoopbackAudioDeviceModuleCreator()
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)>;

// Owns both the existing loopback ADM and its task-queue factory. The callback
// runs on the loopback capture thread and must not block.
class LoopbackAudioTap final {
public:
	using Callback = std::function<void(
		const int16_t*, size_t, size_t, size_t, uint32_t)>;
	explicit LoopbackAudioTap(Callback callback);
	~LoopbackAudioTap();
	[[nodiscard]] bool start();
	void stop();

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace Webrtc
