/**
* @Filename: DBG_print.hpp
* @Author:   Ben Sokol <Ben>
* @Email:    ben@bensokol.com
* @Created:  September 30th, 2019 [8:17pm]
* @Modified: October 1st, 2019 [2:59am]
* @Version:  1.0.0
*
* Copyright (C) 2019 by Ben Sokol. All Rights Reserved.
*/

#ifndef DBG_PRINT_HPP
#define DBG_PRINT_HPP

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>

#include "UTL_timeType.hpp"

namespace DBG {
  namespace INTERNAL {
    class print {
    public:
      print(const bool& aEnable = false, std::ostream& os = std::cerr) : mEnable(aEnable), mFlush(true), mEndl(false) {
        mOS.push_back(&os);
      }

      ~print() {
      }

      template <typename T>
      friend print& operator<<(print& p, T const& val) {
        if (p.mEnable) {
          if (p.mEndl) {
            for (auto& os : p.mOS) {
              (*os) << val << std::endl;
            }
          }
          else if (p.mFlush) {
            for (auto& os : p.mOS) {
              (*os) << val << std::flush;
            }
          }
          else {
            for (auto& os : p.mOS) {
              (*os) << val;
            }
          }
        }
        return p;
      }

      friend print& operator<<(print& p, std::ostream& (*const f)(std::ostream&)) {
        if (p.mEnable) {
          std::ostringstream s;
          s << f;
          for (auto& os : p.mOS) {
            (*os) << s.str();
          }
        }
        return p;
      }

      bool enabled() const {
        return mEnable;
      }

      void enable() {
        mEnable = true;
      }

      void enable(bool aEnable) {
        mEnable = aEnable;
      }

      void disable() {
        mEnable = false;
      }

      void pushOS(std::ostream& os) {
        mOS.push_back(&os);
      }

      void popOS(std::ostream& os) {
        std::cout << "BEFORE: " << mOS.size() << "\n";
        mOS.remove_if([&](std::ostream* aOS) {
          return (aOS == &os);
        });
        std::cout << "AFTER: " << mOS.size() << "\n";
      }

      void flush(bool aFlush) {
        mFlush = aFlush;
      }

      void endl(bool aEndl) {
        mEndl = aEndl;
      }

      bool open(std::string& filename, std::ios_base::openmode mode = std::ofstream::out | std::ofstream::app) {
        std::shared_ptr<std::ofstream> ofs(new std::ofstream(filename, mode));
        if (ofs.get()->is_open()) {
          mOS.push_back(ofs.get());
          mOS_unique_ptr.push_back(ofs);
          return true;
        }

        return false;
      }

      std::string getTimestamp() {
        if (!mEnable) {
          return "";
        }
        UTL::timeType<size_t> time(UTL::timeType<size_t>::TIME_ZONE::LOCAL);
        auto rawtime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto timeinfo = localtime(&rawtime);
        char buffer[80];
        strftime(buffer, 80, "%h %d, %Y", timeinfo);
        return std::string(buffer) + " " + time.to_string();
      }

    private:
      bool mEnable;
      bool mFlush;
      bool mEndl;
      std::list<std::ostream*> mOS;
      std::list<std::shared_ptr<std::ofstream>> mOS_unique_ptr;
    };

  }  // namespace INTERNAL

  static INTERNAL::print print;

#define DBG_print_f DBG::print << __FILE__ << ":" << __LINE__ << " - "

#define DBG_print_ts DBG::print << DBG::print.getTimestamp() << " - "

#define DBG_print_tsf DBG::print << DBG::print.getTimestamp() << " - " << __FILE__ << ":" << __LINE__ << " - "

}  // namespace DBG

#endif
