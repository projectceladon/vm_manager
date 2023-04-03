/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_UTILS_LOG_H_
#define SRC_UTILS_LOG_H_

#pragma once

#include <string>

#include <boost/log/trivial.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/attributes/current_process_name.hpp>

#ifdef DEBUG
#define LOG_ADD_FILE boost::log::attribute_cast<boost::log::attributes::mutable_constant<std::string>>( \
                     logger::gLogger.get_attributes()["File"]).set( \
                         logger::path_to_filename(__FILE__))

#define LOG_ADD_LINE boost::log::attribute_cast<boost::log::attributes::mutable_constant<int>>( \
                     logger::gLogger.get_attributes()["Line"]).set(__LINE__)

#define LOG_ADD_FUNC boost::log::attribute_cast<boost::log::attributes::mutable_constant<std::string>>( \
                     logger::gLogger.get_attributes()["Func"]).set(__FUNCTION__)

#define DEBUG_OUTPUT LOG_ADD_FILE; \
                     LOG_ADD_LINE; \
                     LOG_ADD_FUNC;
#else
#define DEBUG_OUTPUT
#endif

#define LOG(sev) \
    DEBUG_OUTPUT \
    BOOST_LOG_STREAM_WITH_PARAMS(logger::gLogger, \
                                (::boost::log::keywords::severity = ::boost::log::trivial::sev))

namespace logger {
    namespace logging = boost::log;
    namespace attrs = boost::log::attributes;
    namespace expr = boost::log::expressions;
    namespace src = boost::log::sources;
    namespace keywords = boost::log::keywords;

    inline boost::log::trivial::logger::logger_type &gLogger = boost::log::trivial::logger::get();

    template<typename ValueType>
    ValueType set_get_attrib(const char* name, ValueType value) {
        auto attr = logging::attribute_cast<attrs::mutable_constant<ValueType>>
                        (gLogger.get_attributes()[name]);
        attr.set(value);
        return attr.get();
    }

    inline std::string path_to_filename(std::string path) {
        return path.substr(path.find_last_of("/\\") + 1);
    }

    inline void init(void) {
#ifdef DEBUG
        gLogger.add_attribute("File", attrs::mutable_constant<std::string>(""));
        gLogger.add_attribute("Line", attrs::mutable_constant<int>(0));
        gLogger.add_attribute("Func", attrs::mutable_constant<std::string>(""));
#endif
        gLogger.add_attribute("ProcName", attrs::current_process_name());

        logging::add_console_log(std::cout,
            keywords::format = (
                expr::stream
                << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d_%H:%M:%S.%f")
                << " [" << expr::attr<std::string>("ProcName") << "]"
                << " [" << std::setw(8) << logging::trivial::severity << "] "
#ifdef DEBUG
                << '[' << expr::attr<std::string>("File")
                << ':' << expr::attr<int>("Line") << ""
                << ':' << expr::attr<std::string>("Func") << "()]:  "
#endif
                << expr::smessage));

        logging::add_common_attributes();

        logging::core::get()->set_exception_handler(logging::make_exception_suppressor());
    }

    inline void log2file(const char *file) {
        if (file) {
            logging::add_file_log(
                file,
                keywords::format = (
                    expr::stream
                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d_%H:%M:%S.%f")
                    << " [" << expr::attr<std::string>("ProcName") << "]"
                    << " [" << std::setw(8) << logging::trivial::severity << "] "
#ifdef DEBUG
                    << '[' << expr::attr<std::string>("File")
                    << ':' << expr::attr<int>("Line") << ""
                    << ':' << expr::attr<std::string>("Func") << "()]:  "
#endif
                    << expr::smessage),
                keywords::open_mode = std::ios_base::app,
                keywords::auto_flush = (true));
        }
    }

}  // namespace logger

#endif  // SRC_UTILS_LOG_H_
