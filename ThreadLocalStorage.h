
//          Copyright Jason Boer 2015.
// Distributed under the Boost Software License, Version 1.0.
//
// Boost Software License - Version 1.0 - August 17th, 2003
// 
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license(the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third - parties to whom the Software is furnished to
// do so, all subject to the following :
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine - executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON - INFRINGEMENT.IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#pragma once

#include <cinttypes>
#include <thread>
#include <mutex>
#include <array>
#include <functional>
#include <memory>

#ifdef _MSC_VER

#	define TLS_INLINE_ __forceinline
#	define TLS_NOINLINE_ __declspec(noinline)
#	define TLS_ALIGN_16_ __declspec(align(16))
#	define TLS_THREAD_LOCAL_ __declspec(thread)
#	define tls_offsetof_(type, member) offsetof (type, member)

#ifdef TLS_DEBUG_
#	ifndef TLS_ASSERT_
#	define TLS_ASSERT_(x) ((!(x))?__debugbreak():1)
#	endif
#endif

#if 0
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINDOWS_
/*
typedef VOID (NTAPI * WAITORTIMERCALLBACKFUNC) (PVOID, BOOLEAN );

DWORD WINAPI GetThreadId(_In_ HANDLE Thread);
DWORD WINAPI GetCurrentThreadId(void);
HANDLE WINAPI GetCurrentThread(void);
HANDLE WINAPI GetCurrentProcess(void);

BOOL WINAPI RegisterWaitForSingleObject(_Out_ PHANDLE phNewWaitObject, _In_ HANDLE hObject,
	_In_ WAITORTIMERCALLBACK Callback, _In_opt_ PVOID Context, _In_ ULONG dwMilliseconds, _In_ ULONG dwFlags);

BOOL WINAPI DuplicateHandle(_In_ HANDLE hSourceProcessHandle, _In_ HANDLE hSourceHandle, _In_ HANDLE hTargetProcessHandle,
	_Out_ LPHANDLE lpTargetHandle, _In_ DWORD dwDesiredAccess, _In_ BOOL bInheritHandle, _In_ DWORD dwOptions);

BOOL WINAPI UnregisterWaitEx(_In_ HANDLE WaitHandle, _In_opt_ HANDLE CompletionEvent);
*/
extern "C" {
	__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
	__declspec(dllimport) unsigned long __stdcall GetThreadId(void *);
	__declspec(dllimport) void * __stdcall GetCurrentThread();
	__declspec(dllimport) void * __stdcall GetCurrentProcess();
	__declspec(dllimport) int __stdcall RegisterWaitForSingleObject(void **, void *, void (__stdcall *)(void *, unsigned char), void *, unsigned long, unsigned long);
	__declspec(dllimport) int __stdcall DuplicateHandle(void *, void *, void *, void **, unsigned long, int, unsigned long);
	__declspec(dllimport) int __stdcall UnregisterWaitEx(void *, void *);
}
#endif

extern "C" long __cdecl _InterlockedExchange(long volatile *Target, long Value);
#pragma intrinsic (_InterlockedExchange)

namespace TLS {
	TLS_INLINE_ static int locked_test_and_set (uint32_t & value) { return _InterlockedExchange ((long *)&value, 1);}
	TLS_INLINE_ static void locked_clear (uint32_t & value) {_InterlockedExchange ((long *)&value, 0);}
}

#else

#include <pthread.h>

#	ifdef __APPLE_CC__
#	ifndef TLS_USE_PTHREAD_TLS_
#		define TLS_USE_PTHREAD_TLS_ 1
#	endif
#	endif

#	define tls_offsetof_(type, member) __builtin_offsetof (type, member)

#	define TLS_INLINE_ __attribute__((always_inline))
#	define TLS_NOINLINE_ __attribute__((noinline))

#	define TLS_ALIGN_16_ __attribute__ ((aligned (16)))

#	define TLS_THREAD_LOCAL_ __thread

#ifdef TLS_DEBUG_
#include <signal.h>
#	ifndef TLS_ASSERT_
#	define TLS_ASSERT_(x) do {if(!(x))raise(SIGTRAP);}while(0)
#	endif
#endif

namespace TLS {
	inline static int locked_test_and_set (uint32_t & value) { return __sync_lock_test_and_set(&value, 1);}
	inline static void locked_clear (uint32_t & value) {__sync_lock_release(&value);}
}

#endif

#ifndef TLS_ASSERT_
#	define TLS_ASSERT_(x) (void)(x)
#endif

#ifndef TLS_USE_PTHREAD_TLS_
#	define TLS_USE_PTHREAD_TLS_ 0
#endif

namespace TLS {

	typedef uintptr_t thread_id_t;

	template <typename DATA, uint32_t MAX_THREADS = 1024>
	class BASE {
	public:

		struct data_t {
			thread_id_t _id;
			uint32_t	_index;
			DATA		_data;

			data_t() : _id(0), _index(_details::UNUSED_INDEX), _data() {}
			data_t(uint32_t id) : _id(id), _data() {}
		};

		TLS_INLINE_ static void thread_exit(std::function<void(data_t &data)> && callback) {
			_d._thread_exit_callback = typename _details::callback_t(new std::function<void(data_t &data)>(callback));
		}

		TLS_INLINE_ static void thread_enter(std::function<void(data_t & data)> && callback) {
			_d._thread_enter_callback = typename _details::callback_t(new std::function<void(data_t &data)>(callback));
		}

		TLS_INLINE_ static data_t & acquire()
		{
			uint32_t index = _d.get_index();
			TLS_ASSERT_(index < MAX_THREADS);
			return _d._data[index];
		}

		TLS_INLINE_ static thread_id_t thread_id()
		{
#ifdef TLS_USE_PTHREAD_TLS_
			return _d.thread_id();
#else
			return acquire()._id;
#endif
		}

#ifdef TLS_DEBUG_
		static void debug() {
			TLS_ASSERT_((((intptr_t)&_d) & 0xF) == 0);
			printf("addr %p\n", &_d);
			printf("_lock %u\n", (uint32_t)tls_offsetof_(_details, _lock));
			printf("_pad %u\n", (uint32_t)tls_offsetof_(_details, _pad));
			printf("_data %u\n", (uint32_t)tls_offsetof_(_details, _data));
			printf("_available_ids %u\n", (uint32_t)tls_offsetof_(_details, _available_ids));
			printf("_available_id_index %u\n", (uint32_t)tls_offsetof_(_details, _available_id_index));
		}
#endif

		private:

		struct _details {
		public:

			static const uint32_t UNUSED_INDEX = 0xFFFFFFFF;

			TLS_ALIGN_16_ uint32_t _lock;
			char _pad[64 - sizeof(uint32_t)];

#ifdef _MSC_VER
			TLS_INLINE_ static void * IVH() {return (void *)~(uintptr_t)0;} // INVALID_HANDLE_VALUE
			static std::array<void *, MAX_THREADS> _thread_wait_handles;
#else
			static pthread_key_t _pthread_key;
#endif

			std::array<data_t, MAX_THREADS> _data;
			std::array<uint32_t, MAX_THREADS> _available_ids;
			uint32_t _available_id_index;

			typedef std::unique_ptr<std::function<void(data_t & data)>> callback_t;
			static callback_t _thread_exit_callback;
			static callback_t _thread_enter_callback;

			_details() : _lock(0), _data(), _available_ids(), _available_id_index(0) {

				static_assert(sizeof(thread_id_t) == sizeof(void *), "size fail");
				static_assert(sizeof(std::thread::native_handle_type) == sizeof(void *), "size fail");

				for (auto &x : _available_ids) {
					x = MAX_THREADS - _available_id_index - 1;
					++_available_id_index;
				}

#ifdef _MSC_VER
#	ifdef _WINDOWS_
				TLS_ASSERT_(INVALID_HANDLE_VALUE == IVH());
#	endif
				_thread_wait_handles.fill(IVH());
#else
				int result = pthread_key_create(&_pthread_key, thread_exiting);
				TLS_ASSERT_(result == 0); (void)result;
#endif
			}

			~_details() {
#ifdef _MSC_VER
				for (auto & x : _thread_wait_handles) {
					if (x != IVH())
						unregister_wait_thread_handle(x);
				}
#else
				int result = pthread_key_delete(_pthread_key);
				TLS_ASSERT_(result == 0); (void)result;
#endif
				if (_thread_exit_callback) {
					for (auto &x : _data) {
						if (x._index != UNUSED_INDEX)
							_thread_exit_callback->operator()(x);
					}
				}
			}

			TLS_INLINE_ static thread_id_t thread_id()
			{
#ifdef _MSC_VER
				return (thread_id_t)::GetCurrentThreadId();
#else
				return (thread_id_t)::pthread_self();
#endif
			}

#ifdef _MSC_VER
			static void unregister_wait_thread_handle(void *& twh)
			{
				int result = UnregisterWaitEx(twh, IVH());
				TLS_ASSERT_(result != 0); (void)result;
				twh = IVH();
//				auto error = GetLastError();
//				(void)error;
			}

			static void __stdcall thread_exiting(void * value, unsigned char)
#else
			static void thread_exiting(void * value)
#endif
			{
#ifndef _MSC_VER
				pthread_setspecific(_pthread_key, NULL);
#endif
				uint32_t index = (uint32_t)(uintptr_t)value;
				TLS_ASSERT_(index < MAX_THREADS);
				{
					std::lock_guard<_details> lock(_d);
					TLS_ASSERT_(_d._data[index]._index == index);
					if (_thread_exit_callback)
						_thread_exit_callback->operator()(_d._data[index]);
					_d._data[index]._index = UNUSED_INDEX;
					_d._available_ids[_d._available_id_index++] = index;
					TLS_ASSERT_(_d._available_id_index <= MAX_THREADS);
				}
			}

#if (TLS_USE_PTHREAD_TLS_ != 0)
			TLS_INLINE_ static uint32_t get_index()
			{
				void * specific = pthread_getspecific(_pthread_key);
				if (specific == nullptr) {
					uint32_t index;
					acquire(index);
					return index;
				}
				return (uint32_t)(uintptr_t)specific;
			}
#else
			TLS_INLINE_ static uint32_t get_index()
			{
				static TLS_THREAD_LOCAL_ uint32_t index = ~(uint32_t)0;

				if (index > MAX_THREADS)
					acquire(index);

				return index;
			}
#endif
			TLS_NOINLINE_ static void acquire(uint32_t & index)
			{
				{
					std::lock_guard<_details> lock(_d);
					index = _d._available_ids[--_d._available_id_index];
					_d._data[index]._id = thread_id();
					_d._data[index]._index = index;
					if (_thread_enter_callback)
						_thread_enter_callback->operator()(_d._data[index]);
					TLS_ASSERT_(_d._available_id_index > 0);
					TLS_ASSERT_(index < MAX_THREADS);
				}
#ifdef _MSC_VER
				if (_thread_wait_handles[index] != IVH())
					unregister_wait_thread_handle(_thread_wait_handles[index]); // reusing

#ifndef _WINDOWS_
				typedef void * HANDLE;
#endif
				HANDLE hThread = GetCurrentThread();
				HANDLE hProcess = GetCurrentProcess();
				HANDLE hObject;

				//GENERIC_READ (0x80000000L) GENERIC_WRITE (0x40000000L) TRUE 1 DUPLICATE_SAME_ACCESS 0x00000002
				int result = DuplicateHandle(hProcess, hThread, hProcess, &hObject, 0x80000000L | 0x40000000L, 1, 0x00000002);
				TLS_ASSERT_(result != 0);
				//#define WT_EXECUTEONLYONCE 0x00000008
				result = RegisterWaitForSingleObject(&_thread_wait_handles[index], hObject, thread_exiting, (void*)(uintptr_t)index, 0xFFFFFFFF, 0x00000008);
				TLS_ASSERT_(result != 0);
#else
				pthread_setspecific(_pthread_key, (void*)(uintptr_t)index);
#endif
			}

			TLS_INLINE_ void lock() {
				while (locked_test_and_set(_lock) != 0)
					std::this_thread::yield();
			}

			TLS_INLINE_ void unlock() {
				locked_clear(_lock);
			}

			_details(_details &) = delete;
		};

		static TLS_ALIGN_16_ _details _d;
	};


#ifdef TLS_IMPLEMENT_
	template <typename DATA, uint32_t MAX_THREADS>
	TLS_ALIGN_16_ typename BASE<DATA, MAX_THREADS>::_details BASE<DATA, MAX_THREADS>::_d;

	template <typename DATA, uint32_t MAX_THREADS>
	typename BASE<DATA, MAX_THREADS>::_details::callback_t BASE<DATA, MAX_THREADS>::_details::_thread_exit_callback = nullptr;

	template <typename DATA, uint32_t MAX_THREADS>
	typename BASE<DATA, MAX_THREADS>::_details::callback_t BASE<DATA, MAX_THREADS>::_details::_thread_enter_callback = nullptr;

#ifdef _MSC_VER
	template <typename DATA, uint32_t MAX_THREADS>
	std::array<void *, MAX_THREADS> BASE<DATA, MAX_THREADS>::_details::_thread_wait_handles;
#else
	template <typename DATA, uint32_t MAX_THREADS>
	pthread_key_t BASE<DATA, MAX_THREADS>::_details::_pthread_key;
#endif
#endif

};


