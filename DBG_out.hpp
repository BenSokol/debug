/**
 *@Filename: DBG_out.hpp
 *@Author:   Ben Sokol <Ben>
 *@Email:    ben@bensokol.com
 *@Created:  September 30th, 2019 [8:17pm]
* @Modified: October 9th, 2019 [7:03pm]
 *@Version:  1.0.0
*
 *Copyright (C) 2019 by Ben Sokol. All Rights Reserved.
*/

#ifndef DBG_OUT_HPP
#define DBG_OUT_HPP

#include <atomic>
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

#if __has_include(<source_location>)
  #include <source_location>
  #ifndef std_source_location
    #define std_source_location std::source_location
  #endif
#elif __has_include(<experimental/source_location>)
  #include <experimental/source_location>
  #ifndef std_source_location
    #define std_source_location std::experimental::source_location
  #endif
#endif

#ifndef DBG_OUT_MACROS
  #ifdef std_source_location
    #define DBG_OUT_current_location_get       std_source_location::current()
    #define DBG_OUT_current_location_parameter const std_source_location &location
    #define DBG_OUT_current_location_usage     location.line(), location.file_name(), location.function_name()
  #else
    #define DBG_OUT_current_location_get       __LINE__, __FILE__, __FUNCTION__
    #define DBG_OUT_current_location_parameter const int &line, const std::string &file, const std::string &function
    #define DBG_OUT_current_location_usage     line, file, function
  #endif

  #ifndef NDEBUG
    #define DBG_print(...)  DBG::out::instance().print(DBG_OUT_current_location_get, 0, __VA_ARGS__)
    #define DBG_printf(...) DBG::out::instance().printf(DBG_OUT_current_location_get, 0, __VA_ARGS__)
    #define DBG_write(_printTimestamp, _printLocation, _os, _ofs, ...) \
      DBG::out::instance().write(                                      \
        DBG_OUT_current_location_get, _printTimestamp, _printLocation, _os, _ofs, 0, __VA_ARGS__)
    #define DBG_printv(verbosity, ...) DBG::out::instance().print(DBG_OUT_current_location_get, verbosity, __VA_ARGS__)
    #define DBG_printvf(verbosity, ...) \
      DBG::out::instance().printf(DBG_OUT_current_location_get, verbosity, __VA_ARGS__)
    #define DBG_writev(verbosity, _printTimestamp, _printLocation, _os, _ofs, ...) \
      DBG::out::instance().write(                                                  \
        DBG_OUT_current_location_get, _printTimestamp, _printLocation, _os, _ofs, verbosity, __VA_ARGS__)
  #else
    #define DBG_print(...)
    #define DBG_printf(...)
    #define DBG_write(_printTimestamp, _printLocation, _os, _ofs, ...)
    #define DBG_printv(verbosity, ...)
    #define DBG_printvf(verbosity, ...)
    #define DBG_writev(verbosity, _printTimestamp, _printLocation, _os, _ofs, ...)
  #endif
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

    void shutdown();

    // Enable/Disable
    bool osEnabled();
    void osEnable(const bool &aEnable = true);
    void osDisable();

    // Add modifier to end of output
    void flush(bool aFlush);
    void newline(bool aNewline);

    // Enable/disable OFS
    bool ofsEnabled();
    void ofsEnable(const bool &aEnable = true);
    void ofsDisable();

    uint8_t verbosity();
    void verbosity(size_t aVerbosity);

    // Log file modification
    std::string getLogFilename();

    // Queue status
    void wait();
    size_t remainingMessages();

    // Print Message:
    //   Timestamp = mDefaultTimestamp
    //   Location  = mDefaultLocation
    //   std::cerr = true
    //   ofs       = true (if open)
    template <typename Arg, typename... Args>
    void print(DBG_OUT_current_location_parameter, size_t verbosity, Arg &&arg, Args &&... args) {
      if (mDisable) {
        return;
      }

      std::stringstream ss;

      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mMessages.push(new container(
          ss.str(), mDefaultTimestamp, mDefaultLocation, true, true, DBG_OUT_current_location_usage, verbosity));
      }

      mQueueUpdatedCondition.notify_one();
    }

    // Print Message:
    //   Timestamp = mDefaultTimestamp
    //   Location  = mDefaultLocation
    //   std::cerr = false (if ofs is not open)
    //   ofs       = true (if open)
    template <typename Arg, typename... Args>
    void printf(DBG_OUT_current_location_parameter, size_t verbosity, Arg &&arg, Args &&... args) {
      if (mDisable) {
        return;
      }

      std::stringstream ss;
      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      {
        std::unique_lock<std::mutex> lock1(mQueueMutex);
        mMessages.push(new container(
          ss.str(), mDefaultTimestamp, mDefaultLocation, false, true, DBG_OUT_current_location_usage, verbosity));
      }

      mQueueUpdatedCondition.notify_one();
    }


    // Print Message:
    //   Timestamp = user_defined
    //   Location  = user_defined
    //   std::cerr = user_defined (defaults to true if ofs is closed)
    //   ofs       = user_defined (if ofs is open)
    template <typename Arg, typename... Args>
    void write(DBG_OUT_current_location_parameter,
               const bool _printTimestamp,
               const bool _printLocation,
               const bool _os,
               const bool _ofs,
               size_t verbosity,
               Arg &&arg,
               Args &&... args) {
      if (mDisable) {
        return;
      }

      std::stringstream ss;
      ss << std::forward<Arg>(arg);
      ((ss << std::forward<Args>(args)), ...);

      {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mMessages.push(new container(
          ss.str(), _printTimestamp, _printLocation, _os, _ofs, DBG_OUT_current_location_usage, verbosity));
      }

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
                const std::string &_function,
                const size_t &_verbosity);
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
      const size_t verbosity;
    };

    // Thread used for printing
    void outputThread();

    // Returns timestamp as string
    std::string getTimestamp(std::chrono::system_clock::time_point time);

    std::atomic<bool> mEnable;
    std::atomic<bool> mEnableOS;
    std::atomic<bool> mEnableOFS;

    std::atomic<bool> mDisable;

    std::string mLogFilename;

    std::atomic<bool> mDefaultTimestamp;
    std::atomic<bool> mDefaultLocation;

    std::atomic<bool> mFlush;
    std::atomic<bool> mNewline;

    std::atomic<size_t> mVerbosity;

    std::ofstream mOFS;

    bool mStop;
    std::thread mWorker;
    std::queue<container *> mMessages;
    std::mutex mQueueMutex;
    std::mutex mTaskFinishedMutex;
    std::condition_variable mQueueUpdatedCondition;
    std::condition_variable mTaskFinishedCondition;
  };

}  // namespace DBG

#endif
