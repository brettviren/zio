/** 
    Handle TENS message form.
*/

#ifndef ZIO_TENS_H_SEEN
#define ZIO_TENS_H_SEEN

#include "zio/message.hpp"

namespace zio {

    namespace tens {

        // The default message "form" (which is "TENS"), if none is set.
        extern const char* form;

        const char* type_name(const std::type_info& t);

        template<typename Type>
        const char* dtype() { return type_name(typeid(Type)); }

        /*! Generic version of append. */
        void append(Message& msg, message_t&& data,
                    const std::vector<size_t>& shape,
                    size_t word_size, const char* tn,
                    const zio::json& md = zio::json{});

        /*! Append one array data of given shape to message.
         *
         * - shape :: number of elements in each dimension.
         * 
         * This performs a copy of data.
         */
        template<typename ElementType>
        void append(Message& msg, const ElementType* data,
                    const std::vector<size_t>& shape, const zio::json& md = zio::json{}) {
            size_t nbytes = sizeof(ElementType);
            size_t word = nbytes;
            for (auto s : shape) { nbytes *= s; }
            append(msg, zio::message_t((const void*)data, nbytes),
                   shape, word, type_name(typeid(ElementType)), md);
        }
                    
        /*! Return the tensor at the given index.  
         *
         * Index is into the label object TENS JSON array which may
         * not be the message part index.
         *
         * An empty message is returned on error.
         */
        const zio::message_t& at(const Message& msg, size_t index);

        /*! Return the tensor data at payload index in FORM message.
         *
         * This returns a pointer into the message data.
         *
         * Index over/underflow or type mismatch returns NULL.
         */
        template<typename ElementType>
        const ElementType* at(const Message& msg, size_t index) {
            const zio::message_t& ret = at(msg, index);
            if (ret.empty()) { return nullptr; }
            return (const ElementType*) ret.data();
        }

    }
}

#endif
