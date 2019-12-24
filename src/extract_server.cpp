#include "zio/extract.hpp"

zio::ExtractServer::ExtractServer(portptr_t port)
    : m_port(port)
    , m_credits(0)
    , m_total_credits(0)
    , m_credmsg({{level::undefined, "CRED", ""},{0,0,0}})
    , m_eot(false)
{
    m_port->recv(m_credmsg);
    
}
zio::ExtractServer::~ExtractServer();

        /// Recieve a payload data message and maybe timeout.  Return
        /// true if message recieved.  False is returned if a timeout
        /// occurs or if EOT was sent.  
bool zio::ExtractServer::recv(Message& msg, int timeout=-1);

        /// Return true if end-of-transmission is reached.
bool zio::ExtractServer::eot() const;
