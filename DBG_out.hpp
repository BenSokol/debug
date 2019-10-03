/**
 *@Filename: DBG_out.hpp
 *@Author:   Ben Sokol <Ben>
 *@Email:    ben@bensokol.com
 *@Created:  September 30th, 2019 [8:17pm]
* @Modified: October 2nd, 2019 [10:18pm]
 *@Version:  1.0.0
*
 *Copyright (C) 2019 by Ben Sokol. All Rights Reserved.
*/

#ifndef DBG_OUT_HPP
#define DBG_OUT_HPP

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <ios>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <experimental/source_location>

#if __cpp_lib_source_location
  #define std_source_location std::source_location
#elif __cpp_lib_experimental_source_location
  #define std_source_location std::experimental::source_location
#else
  #error ERROR: Requires std::source_location (-std=c++2a)
#endif

#ifndef DBG_OUT_MACROS
  #define DBG_print(...)  DBG::out::instance().print(std_source_location::current(), __VA_ARGS__)
  #define DBG_printf(...) DBG::out::instance().printf(std_source_location::current(), __VA_ARGS__)
  #define DBG_write(_printTimestamp, _printLocation, _os, _ofs, ...) \
    DBG::out::instance().write(std_source_location::current(), _printTimestamp, _printLocation, _os, _ofs, __VA_ARGS__)
#endif


namespace DBG {
  class out {
  public:
    out();
    ~out();

    static out &instance() {
      static out o;
      return o;
    }

    // Enable/Disable
    bool enabled();
    void enable(const bool &aEnable = true);
    void disable();

    // Enable/Disable
    bool osEnabled();
    void osEnable(const bool &aEnable = true);
    void osDisable();

    // Add modifier to end of output
    void flush(bool aFlush);
    void newline(bool aNewline);

    // Enable/disable OFS
    bool openOFS(const std::string &filename, std::ios_base::openmode mode = std::ofstream::out | std::ofstream::app);
    void closeOFS();

    // Queue status
    void wait();
    size_t remainingMessages();

    // Print Message:
    //   Timestamp = mDefaultTimestamp
    //   Location  = mDefaultLocation
    //   std::cerr = true
    //   ofs       = true (if open)
    template <typename Arg, typename... Args>
    void print(const std_source_location &location, Arg &&arg, Args &&... args) {
      std::unique_lock<std::mutex> lock(mQueueMutex);

      std::stringstream ss;

      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      mMessages.push(new container(ss.str(),
                                   mDefaultTimestamp,
                                   mDefaultLocation,
                                   true,
                                   true,
                                   location.line(),
                                   location.file_name(),
                                   location.function_name()));

      mQueueUpdatedCondition.notify_one();
    }

    // Print Message:
    //   Timestamp = mDefaultTimestamp
    //   Location  = mDefaultLocation
    //   std::cerr = false (if ofs is not open)
    //   ofs       = true (if open)
    template <typename Arg, typename... Args>
    void printf(const std_source_location &location, Arg &&arg, Args &&... args) {
      std::unique_lock<std::mutex> lock1(mQueueMutex);

      std::stringstream ss;
      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      mMessages.push(new container(ss.str(),
                                   mDefaultTimestamp,
                                   mDefaultLocation,
                                   false,
                                   true,
                                   location.line(),
                                   location.file_name(),
                                   location.function_name()));

      mQueueUpdatedCondition.notify_one();
    }


    // Print Message:
    //   Timestamp = user_defined
    //   Location  = user_defined
    //   std::cerr = user_defined (defaults to true if ofs is closed)
    //   ofs       = user_defined (if ofs is open)
    template <typename Arg, typename... Args>
    void write(const std_source_location &location,
               const bool _printTimestamp,
               const bool _printLocation,
               const bool _os,
               const bool _ofs,
               Arg &&arg,
               Args &&... args) {
      std::unique_lock<std::mutex> lock(mQueueMutex);

      std::stringstream ss;
      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      mMessages.push(new container(ss.str(),
                                   _printTimestamp,
                                   _printLocation,
                                   _os,
                                   _ofs,
                                   location.line(),
                                   location.file_name(),
                                   location.function_name()));

      mQueueUpdatedCondition.notify_one();
    }


  private:
    class container {
    public:
      container(const std::string &_str,
                const bool &_printTimestamp,
                const bool &_printLocation,
                const bool &_os,
                const bool &_ofs,
                const int &_line,
                const std::string &_file,
                const std::string &_function);
      container(const container &c);
      container(const container &&c);
      ~container();

      const std::string str;
      const bool printTimestamp;
      const bool printLocation;
      const bool os;
      const bool ofs;
      const std::chrono::system_clock::time_point time;
      const int line;
      const std::string file;
      const std::string function;
    };

    // Thread used for printing
    void outputThread();

    // Returns timestamp as string
    std::string getTimestamp(std::chrono::system_clock::time_point time);

    bool mEnable;
    bool mEnableOS;

    bool mDefaultTimestamp;
    bool mDefaultLocation;

    bool mFlush;
    bool mNewline;

    std::ofstream mOFS;

    bool mStop;
    std::thread mWorker;
    std::queue<container *> mMessages;
    std::mutex mOSMutex;
    std::mutex mQueueMutex;
    std::mutex mTaskFinishedMutex;
    std::condition_variable mQueueUpdatedCondition;
    std::condition_variable mTaskFinishedCondition;
  };

}  // namespace DBG

#endif
