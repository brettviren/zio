#ifndef ZIO_EXCEPTIONS_HPP_SEEN
#define ZIO_EXCEPTIONS_HPP_SEEN

#include <stdexcept>
#include <cstring>
#include <string>

namespace zio {

    // This is heavily cribbed from nlohmann::json!

    /*!
      @brief general exception class for zio
     */

    class exception : public std::exception
    {
      public:
        /// returns the explanatory string
        const char* what() const noexcept override { return m.what(); }
        /// the id of the exception
        const int id;

      protected:
        exception(int id_, const char* what_arg) : id(id_), m(what_arg) {}
        static std::string name(const std::string& ename, int id_)
        {
            return "[zio.exception." + ename + "." + std::to_string(id_) + "] ";
        }

      private:
        std::runtime_error m;
    };

    /*!
      @brief exception indicating a socket error

      This exception is thrown when the underlying socket operations
      indicate an error.  The ID is set to the errno.
    */
    class socket_error : public exception
    {
      public:
        /// Create a socket error with all data and optional extra message.
        static socket_error create(int id_, const char* errmsg,
                                   const char* extra = NULL)
        {
            std::string w = exception::name("socket_error", id_) + errmsg;
            if (extra) {
                w += ": ";
                w += extra;
            }
            return socket_error(id_, w.c_str());
        }

        /// Create a socket error with implicit use of errno/strerr
        /// and optional extra message.
        static socket_error create(const char* extra = NULL)
        {
            return create(errno, strerror(errno), extra);
        }

      private:
        socket_error(int id_, const char* what_arg) : exception(id_, what_arg)
        {
        }
    };

    /*!
      @brief exception indicating message error

      This exception is thrown when some error occurs related to the
      fromation of a zio message.  See also @ref socket_error.

      IDs:
      1. ...

     */
    class message_error : public exception
    {
      public:
        static message_error create(int id_, const char* errmsg,
                                    const char* extra = NULL)
        {
            std::string w = exception::name("message_error", id_) + errmsg;
            if (extra) {
                w += ": ";
                w += extra;
            }
            return message_error(id_, w.c_str());
        }

      private:
        message_error(int id_, const char* what_arg) : exception(id_, what_arg)
        {
        }
    };
}  // namespace zio

#endif
