#pragma once

#define DLL_API __declspec(dllexport)  

#include <functional>
#include <thread>
#include <stdint.h>

class DLL_API CEncodeThread {
public:
	using _SourceDataCallback = std::function<int32_t(unsigned char*, int32_t, void* ctx)>;
	using _SinkDataCallback = std::function<int32_t(unsigned char*, int32_t, void* ctx)>;

	bool init(const char* params);
	void start(_SourceDataCallback ReadNextFrameCallback, void* source_ctx, _SinkDataCallback WriteFrameCallback, void* sink_ctx);
	void stop();
	void join();

private:
	int32_t run();

	bool				m_exited;
	std::thread			m_impl;
	_SourceDataCallback m_source_callback;
	_SinkDataCallback   m_sink_callback;
	void*				m_source_ctx;
	void*				m_sink_ctx;
	char				m_parameter_buf[1024];//-g 1920x1080 -b 3000 -f 30/1
};
