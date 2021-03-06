//this suite tests general node functions
#include <mcapi.h>
#include "utester.h"
#include <time.h>

static mcapi_info_t info;
static mcapi_status_t status;

//normal, successful init
test(basic_init)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    sassert( MCAPI_SUCCESS, status );
    mcapi_finalize( &status );
}

//testing that info parameter produces something intelligible
test(init_info)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    sassert( MCAPI_SUCCESS, status );
    uassert( info.mcapi_version == MCAPI_VERSION );
    uassert( info.organization_id == MCAPI_ORG_ID);
    uassert( info.implementation_version == MCAPI_IMPL_VERSION );
    uassert( info.number_of_domains == MCAPI_MAX_DOMAIN );
    uassert( info.number_of_nodes == MCAPI_MAX_NODE );
    uassert( info.number_of_ports == MCAPI_MAX_PORT);
    mcapi_finalize( &status );
}

//testing that getters produce right values
test(id_getters)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_domain_t domain = mcapi_domain_id_get( &status );
    sassert( MCAPI_SUCCESS, status );
    uassert( domain == 1 );
    mcapi_node_t node = mcapi_node_id_get( &status );
    sassert( MCAPI_SUCCESS, status );
    uassert( node == 2 );
    mcapi_finalize( &status );
}


//info parameter must not be null
test(init_fail_null_info)
    mcapi_initialize( 1, 2, 0, 0, 0, &status );
    sassert( MCAPI_ERR_PARAMETER, status );
    mcapi_finalize( &status );
}

//cannot initialize twice
test(init_fail_double_init)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    sassert( MCAPI_ERR_NODE_INITIALIZED, status );
    mcapi_finalize( &status );
}

//succesfull finalization
test(finalize)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_finalize( &status );
    sassert( MCAPI_SUCCESS, status );
}

//cannot finalize before init
test(fail_finalize)
    mcapi_finalize( &status );
    sassert( MCAPI_ERR_NODE_NOTINIT, status );
}

//cannot call wait before init
test(wait_fail_init)
    mcapi_wait( NULL, NULL, 1001, &status );
    sassert( MCAPI_ERR_NODE_NOTINIT, status );
}

//request paramater must not be null
test(wait_fail_req)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_wait( NULL, NULL, 1001, &status );
    sassert( MCAPI_ERR_REQUEST_INVALID, status );
    mcapi_finalize( &status );
}

//stuff pointing from request parameter must not be null
test(wait_fail_req2)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = MCAPI_NULL;
    mcapi_wait( &request, NULL, 1001, &status );
    sassert( MCAPI_ERR_REQUEST_INVALID, status );
    mcapi_finalize( &status );
}

//used below
mcapi_boolean_t failFun( void* data )
{
    return MCAPI_FALSE;
}

//size parameter must not be null
test(wait_fail_size)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( failFun, NULL );
    mcapi_wait( &request, NULL, 1001, &status );
    sassert( MCAPI_ERR_PARAMETER, status );
    mcapi_finalize( &status );
}

//function field of request must not be null either
test(wait_fail_req_null)
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( NULL, NULL );
    mcapi_wait( &request, NULL, 1001, &status );
    sassert( MCAPI_ERR_REQUEST_INVALID, status );
    mcapi_finalize( &status );
}

//testing timeout
test(wait_timeout)
    mcapi_timeout_t timeout = 100;
    size_t size;
    struct timespec t_start, t_end;

    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( failFun, NULL );
    clock_gettime( CLOCK_MONOTONIC, &t_start );
    mcapi_wait( &request, &size, timeout, &status );
    clock_gettime( CLOCK_MONOTONIC, &t_end );
    sassert( MCAPI_TIMEOUT, status );

    double diff = (double)( t_end.tv_nsec - t_start.tv_nsec );
    diff += ( t_end.tv_sec - t_start.tv_sec ) * 1000000000;
    
    uassert( diff > ( timeout * 1000000 ) );

    mcapi_finalize( &status );
}

//used below
mcapi_boolean_t trueFun( void* data )
{
    return MCAPI_TRUE;
}

//must return immediately if happens immediately
test(wait_ok_imm)
    size_t size;
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( trueFun, NULL );
    mcapi_wait( &request, &size, 0, &status );
    sassert( MCAPI_SUCCESS, status );
    mcapi_finalize( &status );
}

//used below
mcapi_boolean_t delTrueFun( void* data )
{
    static int i = 0;

    if ( ++i < 50 )
        return MCAPI_FALSE;

    return MCAPI_TRUE;
}

//must work also if it takes some time
test(wait_ok)
    size_t size;
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( delTrueFun, NULL );
    mcapi_wait( &request, &size, 100, &status );
    sassert( MCAPI_SUCCESS, status );
    mcapi_finalize( &status );
}

//used below
mcapi_boolean_t lastTrueFun( void* data )
{
    static int i = 0;

    if ( ++i < 150 )
        return MCAPI_FALSE;

    return MCAPI_TRUE;
}

//must work also if becomes complete in last attempt
test(wait_last_ok)
    size_t size;
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( lastTrueFun, NULL );
    mcapi_wait( &request, &size, 150, &status );
    sassert( MCAPI_SUCCESS, status );
    mcapi_finalize( &status );
}

//used below
mcapi_boolean_t secondTrueFun( void* data )
{
    static int i = 0;

    ++i;

    if ( i < 2 )
        return MCAPI_FALSE;

    return MCAPI_TRUE;
}

//test if every request possible may be obtained, then released and then obtained
//again. Will not release second time, as next initialization will free everything
test(wait_stress)
    mcapi_request_t request[MCAPI_MAX_REQUESTS];
    mcapi_request_t request_fail;
    int i = 0;
    size_t size;

    mcapi_initialize( 1, 2, 0, 0, &info, &status );

    for ( i = 0; i < MCAPI_MAX_REQUESTS; ++i )
    {
        request[i] = reserve_request( trueFun, NULL );
        uassert( request[i] != MCAPI_NULL );
    }

    request_fail = reserve_request( trueFun, NULL );
    uassert( request_fail == MCAPI_NULL );

    for ( i = 0; i < MCAPI_MAX_REQUESTS; ++i )
    {
        mcapi_wait( &request[i], &size, 150, &status );
        sassert( MCAPI_SUCCESS, status );
    }

    for ( i = 0; i < MCAPI_MAX_REQUESTS; ++i )
    {
        request[i] = reserve_request( trueFun, NULL );
        uassert( request[i] != MCAPI_NULL );
    }

    request_fail = reserve_request( trueFun, NULL );
    uassert( request_fail == MCAPI_NULL );

    mcapi_finalize( &status );
}


//must return immediately after first look
test(test)
    size_t size;
    mcapi_boolean_t res;
    mcapi_initialize( 1, 2, 0, 0, &info, &status );
    mcapi_request_t request = reserve_request( secondTrueFun, NULL );
    res = mcapi_test( &request, &size, &status );
    sassert( MCAPI_PENDING, status );
    uassert( res == MCAPI_FALSE );
    mcapi_finalize( &status );
}

//an off-limit status code is supposed to yield null message
test(status_off_limit)
    char* res = mcapi_display_status( MCAPI_STATUSCODE_END+1, NULL, 0 );
    uassert( res == NULL );
    res = mcapi_display_status( -1, NULL, 0 );
    uassert( res == NULL );
}

//status message is supposed to be null terminated, even if buf is too small
test(status_null_term_short)
    char res[5];
    mcapi_display_status( MCAPI_STATUSCODE_END, res, 5 );
    uassert( res[4] == '\0' );
}

//status message is supposed to be null terminated, even if buf is too big
test(status_null_term_long)
    char res[MCAPI_MAX_STATUS_MSG_LEN+1];
    mcapi_display_status( MCAPI_STATUSCODE_END, res, MCAPI_MAX_STATUS_MSG_LEN+1 );
    uassert( res[MCAPI_MAX_STATUS_MSG_LEN] == '\0' );
}

void suite_node()
{
    dotest(basic_init)
    dotest(init_info)
    dotest(id_getters)
    dotest(init_fail_null_info)
    dotest(init_fail_double_init)
    dotest(finalize)
    dotest(fail_finalize)
    dotest(wait_fail_init)
    dotest(wait_fail_req)
    dotest(wait_fail_req2)
    dotest(wait_fail_size)
    dotest(wait_fail_req_null)
    dotest(wait_timeout)
    dotest(wait_ok_imm)
    dotest(wait_ok)
    dotest(wait_last_ok)
    dotest(wait_stress)
    dotest(test)
    dotest(status_off_limit)
    dotest(status_null_term_short)
    dotest(status_null_term_long)
}
