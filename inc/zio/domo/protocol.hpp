/*! ZIO domo protocol constants 
 *
 * This header holds 7/MDP constants.
 */

#ifndef ZIO_DOMO_PROTOCOL_HPP_SEEN
#define ZIO_DOMO_PROTOCOL_HPP_SEEN

namespace zio {
namespace domo {
namespace mdp {
namespace client {
    // Identify the type and version of the client sub-protocol
    inline const char* ident = "MDPC01";
}
namespace worker {
    // Identify the type and version of the worker sub-protocol
    inline const char* ident = "MDPW01";
    
    /// Worker commands as strings
    inline const char* ready = "\001";
    inline const char* request ="\002";
    inline const char* reply = "\003";
    inline const char* heartbeat = "\004";
    inline const char* disconnect = "\005";
}}}}

#endif
