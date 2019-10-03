/**
* @Filename: DBG_out.cpp
* @Author:   Ben Sokol <Ben>
* @Email:    ben@bensokol.com
* @Created:  October 2nd, 2019 [4:23pm]
* @Modified: October 2nd, 2019 [10:01pm]
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

namespace DBG {
  out::container::container(const std::string &_str,
                            const bool &_printTimestamp,
                            const bool &_printLocation,
                            const bool &_os,
                            const bool &_ofs,
                            const int &_line,
                            const std::string &_file,
                            const std::string &_function) :
      str(_str),
      printTimestamp(_printTimestamp),
      printLocation(_printLocation),
      os(_os),
      ofs(_ofs),
      time(std::chrono::system_clock::now()),
      line(_line),
      file(_file),
      function(_function) {
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
      function(c.function) {
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
      function(std::move(c.function)) {
  }

  out::container::~container() {
  }


  out::out() :
      mEnable(false),
      mEnableOS(true),
      mDefaultTimestamp(true),
      mDefaultLocation(true),
      mFlush(true),
      mNewline(false),
      mStop(false),
      mMessages(std::queue<container *>()) {
    // Initialize queue of messages

    // Start worker
    mWorker = std::thread(&out::outputThread, this);
  }


  out::~out() {
    {
      std::unique_lock<std::mutex> lock(mQueueMutex);
      mStop = true;
    }

    mQueueUpdatedCondition.notify_one();
    mWorker.join();

    while (!mMessages.empty()) {
      delete mMessages.front();
      mMessages.pop();
    }

    if (mOFS.is_open()) {
      mOFS.close();
    }
  }


  bool out::enabled() {
    std::unique_lock<std::mutex> lock(mOSMutex);
    return mEnable;
  }


  void out::enable(const bool &aEnable) {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mEnable = aEnable;
  }


  void out::disable() {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mEnable = false;
  }


  bool out::osEnabled() {
    std::unique_lock<std::mutex> lock(mOSMutex);
    return mEnableOS;
  }


  void out::osEnable(const bool &aEnable) {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mEnableOS = aEnable;
  }


  void out::osDisable() {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mEnableOS = false;
  }


  void out::flush(bool aFlush) {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mFlush = aFlush;
  }


  void out::newline(bool aNewline) {
    std::unique_lock<std::mutex> lock(mOSMutex);
    mNewline = aNewline;
  }


  bool out::openOFS(const std::string &filename, std::ios_base::openmode mode) {
    std::unique_lock<std::mutex> lock(mOSMutex);
    if (mOFS.is_open()) {
      return false;
    }

    mOFS.open(filename, mode);
    return mOFS.is_open();
  }


  void out::closeOFS() {
    std::unique_lock<std::mutex> lock(mOSMutex);
    if (mOFS.is_open()) {
      mOFS.close();
    }
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

      {
        std::unique_lock<std::mutex> lock(mOSMutex);
        std::string outputStr = "";

        if (c->printTimestamp == true) {
          outputStr += getTimestamp(c->time) + " - ";
        }

        if (c->printLocation == true) {
          outputStr += c->file + ":" + c->function + ":" + std::to_string(c->line) + " - ";
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

        if (c->ofs && mOFS.is_open()) {
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
    std::lock(mOSMutex, mQueueMutex);
    std::lock_guard<std::mutex> lock1(mOSMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(mQueueMutex, std::adopt_lock);
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
