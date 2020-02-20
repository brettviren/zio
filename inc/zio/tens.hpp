/** 
    Handle TENS message form.
*/

#ifndef ZIO_TENS_H_SEEN
#define ZIO_TENS_H_SEEN

#include "zio/message.hpp"

namespace zio {

    namespace tens {

        // The message format.
        const char* form = "TENS";

        /*! Generic version of append.  
         *
         * Use templated version for easier interface. */
        void append(Message& msg, std::byte* data, const std::vector<size_t>& shape,
                    size_t word_size, const std::type_info& t);

        /*! Append one array data of given shape to message.
         *
         * - shape :: number of elements in each dimension.
         * 
         * Note: the ordering of each dimension (ie, row-major vs
         * column-major for 2D arrays) is not saved to the message.
         */
        template<typename ElementType>
        void append(Message& msg, const ElementType* data, const std::vector<size_t>& shape) {
            append(msg, (std::byte*)data, shape, sizeof(ElementType), typeid(ElementType));
        }
                    
        /*! Generic version of at().  
         *
         * Use templated version for easier interface. */
        const std::byte* at(const Message& msg, size_t index, const std::type_info& t);

        /*! Return the tensor data at payload index in FORM message.
         *
         * This returns a pointer into the message data.
         *
         * Index over/underflow or type mismatch returns NULL.
         */
        template<typename ElementType>
        const ElementType* at(const Message& msg, size_t index) {
            return (const ElementType*) at(msg, index, typeid(ElementType));
        }

    }
}

#endif
