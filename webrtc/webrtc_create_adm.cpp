// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "webrtc/webrtc_create_adm.h"

#include "webrtc/details/webrtc_openal_adm.h"

#include <api/make_ref_counted.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/include/audio_device_defines.h>
#include <modules/audio_device/include/audio_device_factory.h>


#ifdef WEBRTC_WIN
#include "webrtc/platform/win/webrtc_loopback_adm_win.h"
#elif defined WEBRTC_LINUX // WEBRTC_WIN
#include "webrtc/platform/linux/webrtc_loopback_adm_linux.h"
#elif defined WEBRTC_MAC // WEBRTC_LINUX
#include "webrtc/platform/mac/webrtc_loopback_adm_mac.h"
#endif // WEBRTC_WIN || WEBRTC_LINUX || WEBRTC_MAC

namespace Webrtc {

rtc::scoped_refptr<webrtc::AudioDeviceModule> CreateAudioDeviceModule(
		webrtc::TaskQueueFactory *factory,
		Fn<void(Fn<void(DeviceResolvedId)>)> saveSetDeviceIdCallback,
		CaptureProcessor processor) {
	auto result = rtc::make_ref_counted<details::AudioDeviceOpenAL>(
		factory, std::move(processor));
	if (!result || result->Init() != 0) {
		return nullptr;
	}
	saveSetDeviceIdCallback(result->setDeviceIdCallback());
	return result;
}

auto AudioDeviceModuleCreator(
	Fn<void(Fn<void(DeviceResolvedId)>)> saveSetDeviceIdCallback,
	CaptureProcessor processor)
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)> {
	return [=](webrtc::TaskQueueFactory *factory) {
		return CreateAudioDeviceModule(factory, saveSetDeviceIdCallback, processor);
	};
}

AudioDeviceModulePtr CreateLoopbackAudioDeviceModule(
		webrtc::TaskQueueFactory* factory) {
#ifdef WEBRTC_WIN
	auto result = rtc::make_ref_counted<details::AudioDeviceLoopbackWin>(
		factory);
	if (result->Init() == 0) {
		return result;
	}
#elif defined WEBRTC_LINUX // WEBRTC_WIN
	auto result = rtc::make_ref_counted<details::AudioDeviceLoopbackLinux>(
		factory);
	if (result->Init() == 0) {
		return result;
	}
#elif defined WEBRTC_MAC // WEBRTC_LINUX
	auto result = rtc::make_ref_counted<details::AudioDeviceLoopbackMac>(
		factory);
	if (result->Init() == 0) {
		return result;
	}
#endif // WEBRTC_WIN || WEBRTC_LINUX || WEBRTC_MAC
	return nullptr;
}

auto LoopbackAudioDeviceModuleCreator()
-> std::function<AudioDeviceModulePtr(webrtc::TaskQueueFactory*)> {
	return CreateLoopbackAudioDeviceModule;
}

bool LoopbackAudioCaptureSupported() {
#ifdef WEBRTC_WIN
	return true;
#elif defined WEBRTC_LINUX // WEBRTC_WIN
	return details::AudioDeviceLoopbackLinux::IsSupported();
#elif defined WEBRTC_MAC // WEBRTC_LINUX
	return details::AudioDeviceLoopbackMac::IsSupported();
#else // WEBRTC_WIN || WEBRTC_LINUX || WEBRTC_MAC
	return false;
#endif // WEBRTC_WIN || WEBRTC_LINUX || WEBRTC_MAC
}

class LoopbackAudioTap::Impl final : public webrtc::AudioTransport {
public:
	explicit Impl(Callback callback) : _callback(std::move(callback)) {}
	~Impl() { stop(); }

	bool start() {
		if (_adm) return true;
		_factory = webrtc::CreateDefaultTaskQueueFactory();
		_adm = CreateLoopbackAudioDeviceModule(_factory.get());
		if (!_adm || _adm->RegisterAudioCallback(this) != 0
			|| _adm->InitRecording() != 0
			|| _adm->StartRecording() != 0) {
			stop();
			return false;
		}
		return true;
	}
	void stop() {
		if (_adm) {
			_adm->StopRecording();
			_adm->RegisterAudioCallback(nullptr);
			_adm->Terminate();
			_adm = nullptr;
		}
		_factory.reset(); // The ADM is gone before its factory.
	}

private:
	int32_t RecordedDataIsAvailable(
			const void *samples, size_t frames, size_t bytesPerSample,
			size_t channels, uint32_t rate, uint32_t, int32_t,
			uint32_t micLevel, bool, uint32_t &newMicLevel) override {
		newMicLevel = micLevel;
		_callback(static_cast<const int16_t*>(samples), frames,
			bytesPerSample, channels, rate);
		return 0;
	}
	int32_t NeedMorePlayData(
			size_t, size_t, size_t,
			uint32_t, void *, size_t &out,
			int64_t *, int64_t *) override {
		out = 0; // Capture-only tap, like AudioInputTester.
		return 0;
	}
	void PullRenderData(int, int, size_t, size_t, void *,
			int64_t *, int64_t *) override {}

	Callback _callback;
	std::unique_ptr<webrtc::TaskQueueFactory> _factory;
	AudioDeviceModulePtr _adm;
};

LoopbackAudioTap::LoopbackAudioTap(Callback callback)
: _impl(std::make_unique<Impl>(std::move(callback))) {
}

LoopbackAudioTap::~LoopbackAudioTap() = default;

bool LoopbackAudioTap::start() { return _impl->start(); }

void LoopbackAudioTap::stop() { _impl->stop(); }

} // namespace Webrtc
