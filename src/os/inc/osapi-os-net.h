/*
 *  NASA Docket No. GSC-18,370-1, and identified as "Operating System Abstraction Layer"
 *
 *  Copyright (c) 2019 United States Government as represented by
 *  the Administrator of the National Aeronautics and Space Administration.
 *  All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * File: osapi-os-net.h
 *
 * Author:  Alan Cudmore Code 582
 *
 * Purpose: Contains functions prototype definitions and variables declarations
 *          for the OS Abstraction Layer, Network Module
 */

#ifndef _osapi_network_
#define _osapi_network_

/* NOTE - osconfig.h may optionally specify the value for OS_SOCADDR_MAX_LEN */
#include <osconfig.h>

/*
 * The absolute maximum size of a network address
 *
 * The actual size varies by address type.
 *
 * This definition should be large enough to capture the
 * largest address supported on the system.  The default
 * value here should be suitable for many use-cases while
 * still keeping the address structure size within reason.
 *
 * The user may also provide a tuned value through osconfig.h
 */
#ifndef OS_SOCKADDR_MAX_LEN
#define OS_SOCKADDR_MAX_LEN 28
#endif

/*
 * --------------------------------------------------------------------------------------
 * Sockets API Data Types
 *
 * These data types are defined regardless of whether or not networking is compiled-in
 *
 * They are local to OSAL, they do not directly map to the library-provided types, and
 * they do not occupy any space in the executable
 * --------------------------------------------------------------------------------------
 */

/** @brief Socket domain */
typedef enum
{
    OS_SocketDomain_INVALID, /**< @brief Invalid */
    OS_SocketDomain_INET,    /**< @brief IPv4 address family, most commonly used) */
    OS_SocketDomain_INET6,   /**< @brief IPv6 address family, depends on OS/network stack support */
    OS_SocketDomain_MAX      /**< @brief Maximum */
} OS_SocketDomain_t;

/** @brief Socket type */
typedef enum
{
    OS_SocketType_INVALID,  /**< @brief Invalid */
    OS_SocketType_DATAGRAM, /**< @brief A connectionless, message-oriented socket */
    OS_SocketType_STREAM,   /**< @brief A stream-oriented socket with the concept of a connection */
    OS_SocketType_MAX       /**< @brief Maximum */
} OS_SocketType_t;

/**
 * @brief Storage buffer for generic network address
 *
 * This is a union type that helps to ensure a minimum
 * alignment value for the data storage, such that it can
 * be cast to the system-specific type without
 * increasing alignment requirements.
 */
typedef union
{
    uint8  Buffer[OS_SOCKADDR_MAX_LEN]; /**< @brief Ensures length of at least OS_SOCKADDR_MAX_LEN */
    uint32 AlignU32;                    /**< @brief Ensures uint32 alignment */
    void * AlignPtr;                    /**< @brief Ensures pointer alignment */
} OS_SockAddrData_t;

/**
 * @brief Encapsulates a generic network address
 *
 * This is just an abstract buffer type that holds a network address.
 * It is allocated for the worst-case size defined by OS_SOCKADDR_MAX_LEN,
 * and the real size is stored within.
 */
typedef struct
{
    uint32            ActualLength; /**< @brief Length of the actual address data */
    OS_SockAddrData_t AddrData;     /**< @brief Abstract Address data */
} OS_SockAddr_t;

/**
 * @brief Encapsulates socket properties
 *
 * This is for consistency with other OSAL resource types.
 * Currently no extra properties are exposed here but this
 * could change in a future revision of OSAL as needed.
 */
typedef struct
{
    char      name[OS_MAX_API_NAME]; /**< @brief Name of the socket */
    osal_id_t creator;               /**< @brief OSAL TaskID which opened the socket */
} OS_socket_prop_t;

/**
 * @defgroup OSAPISocketAddr OSAL Socket Address APIs
 *
 * These functions provide a means to manipulate network addresses in a manner that
 * is (mostly) agnostic to the actual network address type.
 *
 * Every network address should be representable as a string (i.e. dotted decimal IP, etc).
 * This can serve as a the "common denominator" to all address types.
 *
 * @{
 */

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Initialize a socket address structure to hold an address of the given family
 *
 * The address is set to a suitable default value for the family.
 *
 * @param[out]  Addr         The address buffer to initialize
 * @param[in]   Domain       The address family
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAddrInit(OS_SockAddr_t *Addr, OS_SocketDomain_t Domain);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Get a string representation of a network host address
 *
 * The specific format of the output string depends on the address family.
 *
 * This string should be suitable to pass back into OS_SocketAddrFromString()
 * which should recreate the same network address, and it should also
 * be meaningful to a user of printed or logged as a C string.
 *
 * @note For IPv4, this would typically be the dotted-decimal format (X.X.X.X).
 *
 * @param[out]  buffer       Buffer to hold the output string
 * @param[in]   buflen       Maximum length of the output string
 * @param[in]   Addr         The network address buffer to convert
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAddrToString(char *buffer, uint32 buflen, const OS_SockAddr_t *Addr);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Set a network host address from a string representation
 *
 * The specific format of the output string depends on the address family.
 *
 * The address structure should have been previously initialized using
 * OS_SocketAddrInit() to set the address family type.
 *
 * @note For IPv4, this would typically be the dotted-decimal format (X.X.X.X).
 * It is up to the discretion of the underlying implementation whether
 * to accept hostnames, as this depends on the availability of DNS services.
 * Since many embedded deployments do not have name services, this should
 * not be relied upon.
 *
 * @param[out]  Addr         The address buffer to initialize
 * @param[in]   string       The string to initialize the address from.
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAddrFromString(OS_SockAddr_t *Addr, const char *string);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Get the port number of a network address
 *
 * For network prototcols that have the concept of a port number (such
 * as TCP/IP and UDP/IP) this function gets the port number from the
 * address structure.
 *
 * @param[out]  PortNum      Buffer to store the port number
 * @param[in]   Addr         The network address buffer
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAddrGetPort(uint16 *PortNum, const OS_SockAddr_t *Addr);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Set the port number of a network address
 *
 * For network prototcols that have the concept of a port number (such
 * as TCP/IP and UDP/IP) this function sets the port number from the
 * address structure.
 *
 * @param[in]   PortNum      The port number to set
 * @param[out]  Addr         The network address buffer
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAddrSetPort(OS_SockAddr_t *Addr, uint16 PortNum);
/**@}*/

/**
 * @defgroup OSALAPISocket OSAL Socket Management APIs
 *
 * These functions are loosely related to the BSD Sockets API but made to be
 * more consistent with other OSAL API functions.  That is, they operate on
 * OSAL IDs (32-bit opaque number values) and return an OSAL error code.
 *
 * OSAL Socket IDs are very closely related to File IDs and share the same ID
 * number space.  Additionally, the file OS_read() / OS_write() / OS_close()
 * calls also work on sockets.
 *
 * Note that all of functions may return #OS_ERR_NOT_IMPLEMENTED if network support
 * is not configured at compile time.
 *
 * @{
 */

/**
 * @brief Opens a socket.
 *
 * A new, unconnected and unbound socket is allocated of the given domain and type.
 *
 * @param[out]  sock_id  Buffer to hold the non-zero OSAL ID
 * @param[in]   Domain   The domain / address family of the socket (INET or INET6, etc)
 * @param[in]   Type     The type of the socket (STREAM or DATAGRAM)
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketOpen(osal_id_t *sock_id, OS_SocketDomain_t Domain, OS_SocketType_t Type);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Binds a socket to a given local address.
 *
 * The specified socket will be bound to the local address and port, if available.
 *
 * If the socket is connectionless, then it only binds to the local address.
 *
 * If the socket is connection-oriented (stream), then this will also put the
 * socket into a listening state for incoming connections at the local address.
 *
 * @param[in]   sock_id  The socket ID
 * @param[in]   Addr     The local address to bind to
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketBind(osal_id_t sock_id, const OS_SockAddr_t *Addr);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Connects a socket to a given remote address.
 *
 * The socket will be connected to the remote address and port, if available.
 * This only applies to stream-oriented sockets.  Calling this on a datagram
 * socket will return an error (these sockets should use SendTo/RecvFrom).
 *
 * @param[in]   sock_id  The socket ID
 * @param[in]   Addr     The remote address to connect to
 * @param[in]   timeout  The maximum amount of time to wait, or OS_PEND to wait forever
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketConnect(osal_id_t sock_id, const OS_SockAddr_t *Addr, int32 timeout);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Waits for and accept the next incoming connection on the given socket
 *
 * This is used for sockets operating in a "server" role.  The socket must be
 * a stream type (connection-oriented) and previously bound to a local address
 * using OS_SocketBind().  This will block the caller up to the given timeout
 * or until an incoming connection request occurs, whichever happens first.
 *
 * The new stream connection is then returned to the caller and the original
 * server socket ID can be reused for the next connection.
 *
 * @param[in]   sock_id      The server socket ID, previously bound using OS_SocketBind()
 * @param[out]  connsock_id  The connection socket, a new ID that can be read/written
 * @param[in]   Addr         The remote address of the incoming connection
 * @param[in]   timeout      The maximum amount of time to wait, or OS_PEND to wait forever
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_SocketAccept(osal_id_t sock_id, osal_id_t *connsock_id, OS_SockAddr_t *Addr, int32 timeout);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Reads data from a message-oriented (datagram) socket
 *
 * If a message is already available on the socket, this should immediately return
 * that data without blocking.  Otherwise, it may block up to the given timeout.
 *
 * @param[in]   sock_id      The socket ID, previously bound using OS_SocketBind()
 * @param[out]  buffer       Pointer to message data receive buffer
 * @param[in]   buflen       The maximum length of the message data to receive
 * @param[out]  RemoteAddr   Buffer to store the remote network address (may be NULL)
 * @param[in]   timeout      The maximum amount of time to wait, or OS_PEND to wait forever
 *
 * @return Count of actual bytes received or error status, see @ref OSReturnCodes
 */
int32 OS_SocketRecvFrom(osal_id_t sock_id, void *buffer, uint32 buflen, OS_SockAddr_t *RemoteAddr, int32 timeout);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Sends data to a message-oriented (datagram) socket
 *
 * This sends data in a non-blocking mode.  If the socket is not currently able to
 * queue the message, such as if its outbound buffer is full, then this returns
 * an error code.
 *
 * @param[in]   sock_id      The socket ID, which must be of the datagram type
 * @param[in]   buffer       Pointer to message data to send
 * @param[in]   buflen       The length of the message data to send
 * @param[in]   RemoteAddr   Buffer containing the remote network address to send to
 *
 * @return Count of actual bytes sent or error status, see @ref OSReturnCodes
 */
int32 OS_SocketSendTo(osal_id_t sock_id, const void *buffer, uint32 buflen, const OS_SockAddr_t *RemoteAddr);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Gets an OSAL ID from a given name
 *
 * @note OSAL Sockets use generated names according to the address and type.
 *
 * @sa OS_SocketGetInfo()
 *
 * @param[out]  sock_id      Buffer to hold result
 * @param[in]   sock_name    Name of socket to find
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_INVALID_POINTER is id or name are NULL pointers
 * @retval #OS_ERR_NAME_TOO_LONG name length including null terminator greater than #OS_MAX_API_NAME
 * @retval #OS_ERR_NAME_NOT_FOUND if the name was not found in the table
 */
int32 OS_SocketGetIdByName(osal_id_t *sock_id, const char *sock_name);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Gets information about an OSAL Socket ID
 *
 * OSAL Sockets use generated names according to the address and type.
 * This allows applications to find the name of a given socket.
 *
 * @param[in]   sock_id      The socket ID
 * @param[out]  sock_prop    Buffer to hold socket information
 *
 * @return Execution status, see @ref OSReturnCodes
 * @retval #OS_SUCCESS @copybrief OS_SUCCESS
 * @retval #OS_ERR_INVALID_ID if the id passed in is not a valid semaphore
 * @retval #OS_INVALID_POINTER if the count_prop pointer is null
 */
int32 OS_SocketGetInfo(osal_id_t sock_id, OS_socket_prop_t *sock_prop);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief  Gets the network ID of the local machine
 *
 * The ID is an implementation-defined value and may not be consistent
 * in meaning across different platform types.
 *
 * @note This API may be removed in a future version of OSAL due to
 *       inconsistencies between platforms.
 *
 * @return The ID or fixed value of -1 if the host id could not be found.
 *         Note it is not possible to differentiate between error codes and valid
 *         network IDs here. It is assumed, however, that -1 is never a valid ID.
 */
int32 OS_NetworkGetID(void);

/*-------------------------------------------------------------------------------------*/
/**
 * @brief Gets the local machine network host name
 *
 * If configured in the underlying network stack,
 * this function retrieves the local hostname of the system.
 *
 * @param[out]  host_name    Buffer to hold name information
 * @param[in]   name_len     Maximum length of host name buffer
 *
 * @return Execution status, see @ref OSReturnCodes
 */
int32 OS_NetworkGetHostName(char *host_name, uint32 name_len);
/**@}*/

#endif
