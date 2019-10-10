/**
* @Filename: DBG_out.cpp
* @Author:   Ben Sokol <Ben>
* @Email:    ben@bensokol.com
* @Created:  October 2nd, 2019 [4:23pm]
* @Modified: October 9th, 2019 [7:04pm]
* @Version:  1.0.0
*
* Copyright (C) 2019 by Ben Sokol. All Rights Reserved.
*/

#include <ctime>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "DBG_out.hpp"

#include "UTL_timeType.hpp"

#if __has_include(<filesystem>)
  #include <filesystem>
  #ifndef std_filesystem
    #define std_filesystem std::filesystem
  #endif
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  #ifndef std_filesystem
    #define std_filesystem std::experimental::filesystem
  #endif
#else
  #error Requires std::filesystem or std::experimental::filesystem
#endif

namespace DBG {
  out::container::container(const std::string &_str,
                            const bool &_printTimestamp,
                            const bool &_printLocation,
                            const bool &_os,
                            const bool &_ofs,
                            const int &_line,
                            const std::string &_file,
                            const std::string &_function,
                            const size_t &_verbosity) :
      str(_str),
      printTimestamp(_printTimestamp),
      printLocation(_printLocation),
      os(_os),
      ofs(_ofs),
      time(std::chrono::system_clock::now()),
      line(_line),
      file(_file),
      function(_function),
      verbosity(_verbosity) {
  }

  out::container::container(const out::container &c) :
      str(c.str),
      printTimestamp(c.printTimestamp),
      printLocation(c.printLocation),
      os(c.os),
      ofs(c.ofs),
      time(c.time),
      line(c.line),
      file(c.file),
      function(c.function),
      verbosity(c.verbosity) {
  }

  out::container::container(const out::container &&c) :
      str(std::move(c.str)),
      printTimestamp(std::move(c.printTimestamp)),
      printLocation(std::move(c.printLocation)),
      os(std::move(c.os)),
      ofs(std::move(c.ofs)),
      time(std::move(c.time)),
      line(std::move(c.line)),
      file(std::move(c.file)),
      function(std::move(c.function)),
      verbosity(std::move(c.verbosity)) {
  }

  out::container::~container() {
  }


  out::out() :
      mEnable(false),
      mEnableOS(false),
      mEnableOFS(false),
      mDefaultTimestamp(true),
      mDefaultLocation(true),
      mFlush(true),
      mNewline(false),
      mVerbosity(0),
      mStop(false),
      mMessages(std::queue<container *>()) {
    // Create logs folder
    std_filesystem::path path = std_filesystem::current_path();
    path += std_filesystem::path("/logs");
    std_filesystem::create_directory(path);

    // Set path to log file
    path += std_filesystem::path(
      "/Debug Log " + std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) + ".log");

    // Open log file
    mOFS.open(path.c_str(), std::ofstream::out | std::ofstream::app);

    // Start worker
    mWorker = std::thread(&out::outputThread, this);
  }  // namespace DBG


  out::~out() {
    {
      std::unique_lock<std::mutex> lock(mQueueMutex);
      mStop = true;
    }

    mQueueUpdatedCondition.notify_one();
    if (mWorker.joinable()) {
      mWorker.join();
    }

    while (!mMessages.empty()) {
      delete mMessages.front();
      mMessages.pop();
    }

    if (mOFS.is_open()) {
      mOFS.close();
    }
  }


  bool out::enabled() {
    return mEnable;
  }


  void out::enable(const bool &aEnable) {
    mEnable = aEnable;
  }


  void out::disable() {
    mEnable = false;
  }


  void out::shutdown() {
    {
      std::unique_lock<std::mutex> lock(mQueueMutex);
      mStop = true;
    }

    mDisable = true;

    mQueueUpdatedCondition.notify_one();
    if (mWorker.joinable()) {
      mWorker.join();
    }

    while (!mMessages.empty()) {
      delete mMessages.front();
      mMessages.pop();
    }

    if (mOFS.is_open()) {
      mOFS.close();
    }
  }


  bool out::osEnabled() {
    return mEnableOS;
  }


  void out::osEnable(const bool &aEnable) {
    mEnableOS = aEnable;
  }


  void out::osDisable() {
    mEnableOS = false;
  }


  bool out::ofsEnabled() {
    return mEnableOFS && mOFS.is_open();
  }


  void out::ofsEnable(const bool &aEnable) {
    mEnableOFS = aEnable && mOFS.is_open();
  }


  void out::ofsDisable() {
    mEnableOFS = false;
  }


  uint8_t out::verbosity() {
    return mVerbosity;
  }


  void out::verbosity(size_t aVerbosity) {
    mVerbosity = aVerbosity;
  }


  std::string out::getLogFilename() {
    mEnableOFS = false;
    return mLogFilename;
  }


  void out::flush(bool aFlush) {
    mFlush = aFlush;
  }


  void out::newline(bool aNewline) {
    mNewline = aNewline;
  }


  void out::outputThread() {
    // run tasks in queue until mStop == true || mMessages.empty()
    for (;;) {
      container *c;

      {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mQueueUpdatedCondition.wait(lock, [this]() {
          return (mStop || (mEnable && !mMessages.empty()));
        });

        // End the worker thread immediately if it is asked to stop
        if (mStop) {
          return;
        }
        else {
          c = mMessages.front();
        }
      }

      if (mVerbosity >= c->verbosity) {
        std::string outputStr = "";

        if (c->printTimestamp == true) {
          outputStr += getTimestamp(c->time) + " - ";
        }

        if (c->printLocation == true) {
          outputStr += c->file + ":" + c->function + ":" + std::to_string(c->line) + "\t - ";
        }

        outputStr += c->str;

        if (mNewline) {
          outputStr += "\n";
        }

        if (c->os && mEnableOS) {
          std::cerr << outputStr;
          if (mFlush) {
            std::cerr << std::flush;
          }
        }

        if (c->ofs && mEnableOFS && mOFS.is_open()) {
          mOFS << outputStr;
          if (mFlush) {
            mOFS << std::flush;
          }
        }
      }

      {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        delete c;
        mMessages.pop();
      }

      mTaskFinishedCondition.notify_all();
    }
  }


  void out::wait() {
    while (!mMessages.empty()) {
      std::unique_lock<std::mutex> lock(mTaskFinishedMutex);
      mTaskFinishedCondition.wait(lock, [this]() {
        return mMessages.empty();
      });
    }
  }


  size_t out::remainingMessages() {
    std::unique_lock<std::mutex> lock(mQueueMutex);
    return mMessages.size();
  }


  std::string out::getTimestamp(std::chrono::system_clock::time_point time) {
    UTL::timeType<size_t> time_type(UTL::timeType<size_t>::TIME_ZONE::LOCAL, time);
    auto rawtime = std::chrono::system_clock::to_time_t(time);
    auto timeinfo = localtime(&rawtime);
    char buffer[80];
    strftime(buffer, 80, "%h %d, %Y", timeinfo);
    return std::string(buffer) + " " + time_type.to_string();
  }
}  // namespace DBG
