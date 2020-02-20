/** 
    Handle TENS message form.
*/

#ifndef ZIO_TENS_H_SEEN
#define ZIO_TENS_H_SEEN

#include "zio/message.hpp"

// interned copy of https://github.com/rogersce/cnpy
#include "zio/cnpy.hpp"

namespace zio {

    namespace tens {

        const char* form = "TENS";

        /*! Return a JSON object holding metadata for a tensor
         */
        zio::json metaobj(const cnpy::NpyArray& tensor);

        /*! Initialize a zio::Message to be used as TENS form.
         */
        void init(Message& msg);

        /*! Append copy of numpy array data and metadata to TENS message.
         */
        void append(Message& msg, const cnpy::NpyArray& tensor);

        /*! Return the tensor at payload index in FORM message
         */
        cnpy::NpyArray at(const Message& msg, size_t index);

    }
}

#endif
