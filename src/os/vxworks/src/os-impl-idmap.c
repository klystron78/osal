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

/**
 * \file     os-impl-idmap.c
 * \ingroup  vxworks
 * \author   joseph.p.hickey@nasa.gov
 *
 */
/****************************************************************************************
                                    INCLUDE FILES
****************************************************************************************/

#include "os-vxworks.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

#include <errnoLib.h>
#include <objLib.h>
#include <semLib.h>
#include <sysLib.h>

/****************************************************************************************
                                     DEFINES
****************************************************************************************/

/****************************************************************************************
                                   GLOBAL DATA
****************************************************************************************/

#define GLOBAL_MUTEX_MEM(name) \
    static VX_MUTEX_SEMAPHORE(OS_m_##name); \
    static VX_CONDVAR(OS_c_##name)

GLOBAL_MUTEX_MEM(task_table_mut_sem)
GLOBAL_MUTEX_MEM(queue_table_mut_mem)
GLOBAL_MUTEX_MEM(bin_sem_table_mut_mem)
GLOBAL_MUTEX_MEM(mutex_table_mut_mem)
GLOBAL_MUTEX_MEM(count_sem_table_mut_mem)
GLOBAL_MUTEX_MEM(stream_table_mut_mem)
GLOBAL_MUTEX_MEM(dir_table_mut_mem)
GLOBAL_MUTEX_MEM(timebase_table_mut_mem)
GLOBAL_MUTEX_MEM(timecb_table_mut_mem)
GLOBAL_MUTEX_MEM(module_table_mut_mem)
GLOBAL_MUTEX_MEM(filesys_table_mut_mem)
GLOBAL_MUTEX_MEM(console_mut_mem)

#define GLOBAL_MUTEX_PTR(name) { .m_mem = OS_m_##name, .c_mem = OS_c_##name }

VxWorks_GlobalMutex_t VX_MUTEX_TABLE[] = {
    [OS_OBJECT_TYPE_UNDEFINED]   = {NULL},
    [OS_OBJECT_TYPE_OS_TASK]     = GLOBAL_MUTEX_PTR(task_table_mut_sem),
    [OS_OBJECT_TYPE_OS_QUEUE]    = GLOBAL_MUTEX_PTR(queue_table_mut_mem),
    [OS_OBJECT_TYPE_OS_COUNTSEM] = GLOBAL_MUTEX_PTR(count_sem_table_mut_mem),
    [OS_OBJECT_TYPE_OS_BINSEM]   = GLOBAL_MUTEX_PTR(bin_sem_table_mut_mem),
    [OS_OBJECT_TYPE_OS_MUTEX]    = GLOBAL_MUTEX_PTR(mutex_table_mut_mem),
    [OS_OBJECT_TYPE_OS_STREAM]   = GLOBAL_MUTEX_PTR(stream_table_mut_mem),
    [OS_OBJECT_TYPE_OS_DIR]      = GLOBAL_MUTEX_PTR(dir_table_mut_mem),
    [OS_OBJECT_TYPE_OS_TIMEBASE] = GLOBAL_MUTEX_PTR(timebase_table_mut_mem),
    [OS_OBJECT_TYPE_OS_TIMECB]   = GLOBAL_MUTEX_PTR(timecb_table_mut_mem),
    [OS_OBJECT_TYPE_OS_MODULE]   = GLOBAL_MUTEX_PTR(module_table_mut_mem),
    [OS_OBJECT_TYPE_OS_FILESYS]  = GLOBAL_MUTEX_PTR(filesys_table_mut_mem),
    [OS_OBJECT_TYPE_OS_CONSOLE]  = GLOBAL_MUTEX_PTR(console_mut_mem),
};

enum
{
    VX_MUTEX_TABLE_SIZE = (sizeof(VX_MUTEX_TABLE) / sizeof(VX_MUTEX_TABLE[0]))
};

/*----------------------------------------------------------------
 *
 * Function: OS_Lock_Global_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    VxWorks_GlobalMutex_t *mut;

    if (idtype >= VX_MUTEX_TABLE_SIZE)
    {
        return OS_ERROR;
    }

    mut = &VX_MUTEX_TABLE[idtype];
    if ((mut->m_vxid == SEM_ID_NULL) || (mut->c_vxid == CONDVAR_ID_NULL))
    {
        return OS_ERROR;
    }

    if (semTake(mut->m_vxid, WAIT_FOREVER) != OK)
    {
        OS_DEBUG("semTake() - vxWorks errno %d\n", errno);
        return OS_ERROR;
    }

    return OS_SUCCESS;
} /* end OS_Lock_Global_Impl */

/*----------------------------------------------------------------
 *
 * Function: OS_Unlock_Global_Impl
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    VxWorks_GlobalMutex_t *mut;
    int ret;

    if (idtype >= VX_MUTEX_TABLE_SIZE)
    {
        return OS_ERROR;
    }

    mut = &VX_MUTEX_TABLE[idtype];
    if ((mut->m_vxid == SEM_ID_NULL) || (mut->c_vxid == CONDVAR_ID_NULL))
    {
        return OS_ERROR;
    }

    ret = condVarBroadcast(mut->c_vxid);
    if(ret!=OK) {
        OS_DEBUG("condVarBroadcast failed");
        /* unexpected but keep going (not critical) */
    }

    if (semGive(mut->m_vxid) != OK)
    {
        OS_DEBUG("semGive() - vxWorks errno %d\n", errno);
        return OS_ERROR;
    }

    return OS_SUCCESS;
} /* end OS_Unlock_Global_Impl */

void OS_WaitForStateChange_Impl(osal_objtype_t idtype, uint32 attempts)
{
    VxWorks_GlobalMutex_t *mut;
    _Vx_ticks_t ticks;

    if (idtype < VX_MUTEX_TABLE_SIZE)
    {
        return;
    }

    mut = &VX_MUTEX_TABLE[idtype];
    if ((mut->m_vxid == SEM_ID_NULL) || (mut->c_vxid == CONDVAR_ID_NULL))
    {
        return;
    }

    if(attempts <= 10) {
        ticks = OS_SharedGlobalVars.TicksPerSecond / 100; /* start at 10 ms */
        ticks *= attempts * attempts;
    }
    else {
        /* wait 1 second (max for polling) */
        ticks = OS_SharedGlobalVars.TicksPerSecond;
    }

    /* the posix impl does not check return code */
    condVarWait(mut->c_vxid, mut->m_vxid, ticks);
}

/****************************************************************************************
                                INITIALIZATION FUNCTION
****************************************************************************************/

/*----------------------------------------------------------------
 *
 * Function: OS_VxWorks_TableMutex_Init
 *
 *  Purpose: Initialize the tables that the OS API uses to keep track of information
 *           about objects
 *
 *-----------------------------------------------------------------*/
int32 OS_VxWorks_TableMutex_Init(osal_objtype_t idtype)
{
    int32  return_code = OS_SUCCESS;
    SEM_ID semid;
    CONDVAR_ID condid;

    /* Initialize the table mutex for the given idtype */
    if (idtype < VX_MUTEX_TABLE_SIZE)
    {
        if(VX_MUTEX_TABLE[idtype].m_mem) {
            semid = semMInitialize(VX_MUTEX_TABLE[idtype].m_mem, SEM_Q_PRIORITY | SEM_INVERSION_SAFE);

            if (semid == SEM_ID_NULL)
            {
                OS_DEBUG("Error: semMInitialize() failed - vxWorks errno %d\n", errno);
                return_code = OS_ERROR;
            }
            else
            {
                VX_MUTEX_TABLE[idtype].m_vxid = semid;
            }

            condid = condVarInitialize(VX_MUTEX_TABLE[idtype].c_mem, CONDVAR_Q_PRIORITY);

            if(condid == CONDVAR_ID_NULL)
            {
                OS_DEBUG("Error: condVarInitialize() failed - vxWorks errno %d\n", errno);
                return_code = OS_ERROR;
            }
            else
            {
                VX_MUTEX_TABLE[idtype].c_vxid = condid;
            }
        }
    }

    return (return_code);
} /* end OS_VxWorks_TableMutex_Init */
