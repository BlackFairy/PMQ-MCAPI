#include "pmq_layer.h"
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>

//how many nanos in a milli
#define NANO_IN_MILLI 1000000

//how many nanos in one
#define NANO_IN_ONE 1000000000

//how many millis in one
#define MILLI_IN_ONE 1000

//how many micros in a milli
#define MICRO_IN_MILLI 1000

//this macro sets current time to given time_spec and then adds given
//milliseconds to it.
#define ADD_MILLIS_TO_NOW( time_limit, timeout ) \
    clock_gettime( CLOCK_REALTIME, &time_limit ); \
    time_limit.tv_nsec += (timeout%MILLI_IN_ONE)*NANO_IN_MILLI; \
    time_limit.tv_sec += timeout/MILLI_IN_ONE + \
    time_limit.tv_nsec/NANO_IN_ONE; \
    time_limit.tv_nsec = time_limit.tv_nsec%NANO_IN_ONE;

inline mcapi_status_t pmq_send(
    mqd_t msgq_id, 
    void* buffer, 
    size_t buffer_size, 
    mcapi_priority_t priority,
    mcapi_timeout_t timeout )
{
    //the result of action
    mqd_t result = -1;

    if ( timeout == MCAPI_TIMEOUT_INFINITE )
    {
        //sending the data
        result = mq_send( msgq_id, buffer, buffer_size, priority );
    }
    else
    {
        //timeout used by the posix function
        struct timespec time_limit = { 0, 0 };

        //specify timeout for the call
        ADD_MILLIS_TO_NOW( time_limit, timeout );

        //sending the data
        result = mq_timedsend( msgq_id, buffer, buffer_size,
        priority, &time_limit );
    }

    //check for error
    if ( result == -1 )
    {
        if ( errno != ETIMEDOUT )
        {
            perror("mq_send");
            return MCAPI_ERR_TRANSMISSION;
        }

        //if it was a timeout, we shall return with timeout
        return MCAPI_TIMEOUT;
    }

    return MCAPI_SUCCESS;
}

inline mcapi_status_t pmq_recv(
    mqd_t msgq_id, 
    void* buffer, 
    size_t buffer_size,
    size_t* received_size, 
    mcapi_priority_t* priority,
    mcapi_timeout_t timeout )
{
    if ( timeout == MCAPI_TIMEOUT_INFINITE )
    {
        //receiving the data
        *received_size = mq_receive( msgq_id, buffer, buffer_size, priority );
    }
    else
    {
        //timeout used by posix-function
        struct timespec time_limit = { 0, 0 };

        //specify timeout for the call
        ADD_MILLIS_TO_NOW( time_limit, timeout );

        //receiving the data
        *received_size = mq_timedreceive( msgq_id, buffer,
        buffer_size, priority, &time_limit );
    }

    //check for error
    if ( *received_size == -1 )
    {
        if ( errno != ETIMEDOUT )
        {
            perror("mq_recv");
            return MCAPI_ERR_TRANSMISSION;
        }

        //if it was a timeout, we shall return with timeout
        return MCAPI_TIMEOUT;
    }

    return MCAPI_SUCCESS;
}

inline mcapi_uint_t pmq_avail(
    mqd_t msgq_id,
    mcapi_status_t* mcapi_status )
{
    //the retrieved attributes are obtained here for the check
    struct mq_attr uattr;

    //try to open the attributes...
    if (mq_getattr(msgq_id, &uattr) == -1)
    {
        perror("When obtaining msq attributes to check message count");
        *mcapi_status = MCAPI_ERR_GENERAL;
        return MCAPI_NULL;
    }

    //success
    *mcapi_status = MCAPI_SUCCESS;

    //and return the count
    return uattr.mq_curmsgs;
}

inline mcapi_status_t pmq_create_epd(
    mcapi_endpoint_t endpoint )
{
    //the queue created for endpoint
    mqd_t msgq_id;
    //the attributes to be set for queue
    //Blocking, maximum number of msgs, their max size and current messages
    struct mq_attr attr = { 0, MAX_QUEUE_ELEMENTS, MCAPI_MAX_MESSAGE_SIZE, 0 };
    //the retrieved attributes are obtained here for the check
    struct mq_attr uattr;

    //open the queue for reception, but only create
    msgq_id = mq_open(endpoint->defs->msg_name, O_RDWR | O_CREAT,
    (S_IRUSR | S_IWUSR), &attr);

    //if did not work, then its an error
    if (msgq_id == (mqd_t)-1) {
        perror("When opening msq from create");
        return MCAPI_ERR_GENERAL;
    }

    //try to open the attributes...
    if (mq_getattr(msgq_id, &uattr) == -1)
    {
        perror("When obtaining msq attributes for check");
        return MCAPI_ERR_GENERAL;
    }
    
    //...and check if match
    if ( attr.mq_flags != uattr.mq_flags || attr.mq_maxmsg != uattr.mq_maxmsg
    || attr.mq_msgsize != uattr.mq_msgsize )
    {
        fprintf(stderr, "Set msq attributes do not match!\n");
        return MCAPI_ERR_GENERAL;
    }

    //now, set its message queue
    endpoint->msgq_id = msgq_id;
    //but un-set the channel message queue
    endpoint->chan_msgq_id = -1;

    return MCAPI_SUCCESS;
}

inline mcapi_status_t pmq_open_epd(
    mcapi_endpoint_t endpoint, 
	mcapi_timeout_t timeout )
{
    //the queue to be obtained
    mqd_t msgq_id = -1;
    //how close we are to timeout
    uint32_t ticks = 0;

    //obtaining the receiving queue, until they have created it!
    do
    {
        //sleep a millisecond between iterations
        usleep( MICRO_IN_MILLI );
        //try to open for receive only, but do not create it
        msgq_id = mq_open(endpoint->defs->msg_name, O_WRONLY );
        //closer for time out!
        ++ticks;
    }
    while ( msgq_id == -1 && ( timeout == MCAPI_TIMEOUT_INFINITE ||
    ticks < timeout ) );

    //if it was a timeout, we shall return with timeout
    if ( msgq_id == -1 )
    {
        if ( errno != ENOENT )
        {
            perror("When opening msq from get");
            return MCAPI_ERR_GENERAL;
        }

        return MCAPI_TIMEOUT;
    }

    //now, set the message queue
    endpoint->msgq_id = msgq_id;

    return MCAPI_SUCCESS;
}

inline void pmq_delete_epd(
    mcapi_endpoint_t endpoint )
{
    //needed for the receive call
    char recv_buf[MCAPI_MAX_MESSAGE_SIZE];
    //result of received
    size_t mslen;
    //zero time limit for immediate return
    struct timespec time_limit = { 0, 0 };

    //read out all messages from queue
    do
    {
        mslen = mq_timedreceive( endpoint->msgq_id, recv_buf, 
        MCAPI_MAX_MESSAGE_SIZE, NULL, &time_limit );
    }
    while( mslen != -1 );

    //close the queue
    mq_close( endpoint->msgq_id );
    //nullify the mgsq_id
    endpoint->msgq_id = -1;
}

inline mcapi_boolean_t pmq_open_chan_recv( mcapi_endpoint_t us )
{
    //Blocking, maximum number of msgs, their max size and current number
    struct mq_attr attr = { 0, MAX_QUEUE_ELEMENTS, 0, 0 };
    //the retrieved attributes are obtained here for the check
    struct mq_attr uattr;
    //the queue created for channel
    mqd_t msgq_id;

    //yes: we may switch the size depending on type
    if ( us->defs->type == MCAPI_PKT_CHAN )
    {
        attr.mq_msgsize = MCAPI_MAX_PACKET_SIZE;
    }
    else if ( us->defs->type == MCAPI_NO_CHAN )
    {
        attr.mq_msgsize = MCAPI_MAX_MESSAGE_SIZE;
    }
    else if ( us->defs->type == MCAPI_SCL_CHAN )
    {
        //in case of scalar channel, the channel defined size is used
        size_t scalar_size = us->defs->scalar_size;
        attr.mq_msgsize = scalar_size;

        //Warn if not standard
        if ( scalar_size != 1 && scalar_size != 2 &&
            scalar_size != 4 && scalar_size != 8 )
        {
            fprintf(stderr, "Trying to open scalar channel with "    
            "invalid size!\n");
        }
    }
    else
    {
        fprintf(stderr, "Channel recv open provided with null chan type!\n");
        return MCAPI_FALSE;
    }
    
    //try to open the message queue to be used as channel
    msgq_id = mq_open(us->defs->chan_name, O_RDWR | O_CREAT | O_EXCL,
    (S_IRUSR | S_IWUSR), &attr);

    //if did not work, then its an error
    if ( msgq_id == -1 )
    {
        perror("When opening msq from recv open");

        return MCAPI_FALSE;
    }

    //try to open the attributes...
    if (mq_getattr(msgq_id, &uattr) == -1)
    {
        perror("When obtaining channel msq attributes for check");
        return MCAPI_FALSE;
    }
    
    //...and check if match
    if ( attr.mq_flags != uattr.mq_flags || attr.mq_maxmsg != uattr.mq_maxmsg
    || attr.mq_msgsize != uattr.mq_msgsize )
    {
        fprintf(stderr, "Set channel msq attributes do not match!\n");
        return MCAPI_FALSE;
    }

    //success! assign channel to handle
    us->chan_msgq_id = msgq_id;

    //done
    return MCAPI_TRUE;
}

inline mcapi_boolean_t pmq_open_chan_send( mcapi_endpoint_t our_endpoint )
{
    //the queue to be obtained
    mqd_t msgq_id;

    //no -1 means we already has an message queue
    if ( our_endpoint->chan_msgq_id == -1 )
    {
        //try to open, but do not create it
        msgq_id = mq_open(our_endpoint->defs->chan_name, O_RDWR );

        //failure means retry
        if ( msgq_id == -1 )
        {
            //print only if not non-existing, as we expect it.
            if ( errno != ENOENT )
                perror( "when obtaining channel queue for send");

            return MCAPI_FALSE;
        }

        //success! assign channel to handle
        our_endpoint->chan_msgq_id = msgq_id;
    }

    return MCAPI_TRUE;
}

inline void pmq_delete_chan(
    mcapi_endpoint_t endpoint,
    mcapi_boolean_t unlink )
{
    //close and unlink the queue
    mq_close( endpoint->chan_msgq_id );

    if ( unlink == MCAPI_TRUE )
        mq_unlink( endpoint->defs->chan_name );

    //nullify the mgsq_id
    endpoint->chan_msgq_id = -1;
}
