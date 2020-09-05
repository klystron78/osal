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
 * \file     osapi-idmap.c
 * \ingroup  shared
 * \author   joseph.p.hickey@nasa.gov
 *
 * This file contains utility functions to interpret OSAL IDs
 * in a generic/common manner.  They are used internally within
 * OSAL by all the various modules.
 *
 * In order to add additional verification capabilities, each class of fundamental
 * objects will use its own ID space within the 32-bit integer ID value.  This way
 * one could not mistake a Task ID for a Queue ID or vice versa.  Also, all IDs will
 * become nonzero and an ID of zero is ALWAYS invalid.
 *
 * These functions provide a consistent way to validate a 32-bit OSAL ID as
 * well as determine its internal type and index.
 *
 * The map/unmap functions are not part of the public API -- applications
 * should be treating OSAL IDs as opaque objects.
 *
 * NOTE: The only exception is OS_ConvertToArrayIndex() as this is necessary to
 * assist applications when storing OSAL IDs in a table.
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * User defined include files
 */
#include "os-shared-common.h"
#include "os-shared-idmap.h"
#include "os-shared-task.h"

typedef enum
{
   OS_TASK_BASE = 0,
   OS_QUEUE_BASE = OS_TASK_BASE + OS_MAX_TASKS,
   OS_BINSEM_BASE = OS_QUEUE_BASE + OS_MAX_QUEUES,
   OS_COUNTSEM_BASE = OS_BINSEM_BASE + OS_MAX_BIN_SEMAPHORES,
   OS_MUTEX_BASE = OS_COUNTSEM_BASE + OS_MAX_COUNT_SEMAPHORES,
   OS_STREAM_BASE = OS_MUTEX_BASE + OS_MAX_MUTEXES,
   OS_DIR_BASE = OS_STREAM_BASE + OS_MAX_NUM_OPEN_FILES,
   OS_TIMEBASE_BASE = OS_DIR_BASE + OS_MAX_NUM_OPEN_DIRS,
   OS_TIMECB_BASE = OS_TIMEBASE_BASE + OS_MAX_TIMEBASES,
   OS_MODULE_BASE = OS_TIMECB_BASE + OS_MAX_TIMERS,
   OS_FILESYS_BASE = OS_MODULE_BASE + OS_MAX_MODULES,
   OS_CONSOLE_BASE = OS_FILESYS_BASE + OS_MAX_FILE_SYSTEMS,
   OS_MAX_TOTAL_RECORDS = OS_CONSOLE_BASE + OS_MAX_CONSOLES
} OS_ObjectIndex_t;


/*
 * Global ID storage tables
 */

/* Tables where the OS object information is stored */
static OS_common_record_t OS_common_table[OS_MAX_TOTAL_RECORDS];

typedef struct
{
    /* Keep track of the last successfully-issued object ID of each type */
    osal_id_t last_id_issued;

    /* The last task to lock/own this global table */
    osal_id_t table_owner;
} OS_objtype_state_t;

OS_objtype_state_t OS_objtype_state[OS_OBJECT_TYPE_USER];


OS_common_record_t * const OS_global_task_table       = &OS_common_table[OS_TASK_BASE];
OS_common_record_t * const OS_global_queue_table      = &OS_common_table[OS_QUEUE_BASE];
OS_common_record_t * const OS_global_bin_sem_table    = &OS_common_table[OS_BINSEM_BASE];
OS_common_record_t * const OS_global_count_sem_table  = &OS_common_table[OS_COUNTSEM_BASE];
OS_common_record_t * const OS_global_mutex_table      = &OS_common_table[OS_MUTEX_BASE];
OS_common_record_t * const OS_global_stream_table     = &OS_common_table[OS_STREAM_BASE];
OS_common_record_t * const OS_global_dir_table        = &OS_common_table[OS_DIR_BASE];
OS_common_record_t * const OS_global_timebase_table   = &OS_common_table[OS_TIMEBASE_BASE];
OS_common_record_t * const OS_global_timecb_table     = &OS_common_table[OS_TIMECB_BASE];
OS_common_record_t * const OS_global_module_table     = &OS_common_table[OS_MODULE_BASE];
OS_common_record_t * const OS_global_filesys_table    = &OS_common_table[OS_FILESYS_BASE];
OS_common_record_t * const OS_global_console_table    = &OS_common_table[OS_CONSOLE_BASE];

/*
 *********************************************************************************
 *          IDENTIFIER MAP / UNMAP FUNCTIONS
 *********************************************************************************
 */

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdInit
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           clears the entire table and brings it to a proper initial state
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdInit(void)
{
    memset(OS_common_table, 0, sizeof(OS_common_table));
    memset(OS_objtype_state, 0, sizeof(OS_objtype_state));
    return OS_SUCCESS;
} /* end OS_ObjectIdInit */

/*----------------------------------------------------------------
 *
 * Function: OS_GetMaxForObjectType
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/
uint32 OS_GetMaxForObjectType(uint32 idtype)
{
   switch(idtype)
   {
   case OS_OBJECT_TYPE_OS_TASK:     return OS_MAX_TASKS;
   case OS_OBJECT_TYPE_OS_QUEUE:    return OS_MAX_QUEUES;
   case OS_OBJECT_TYPE_OS_BINSEM:   return OS_MAX_BIN_SEMAPHORES;
   case OS_OBJECT_TYPE_OS_COUNTSEM: return OS_MAX_COUNT_SEMAPHORES;
   case OS_OBJECT_TYPE_OS_MUTEX:    return OS_MAX_MUTEXES;
   case OS_OBJECT_TYPE_OS_STREAM:   return OS_MAX_NUM_OPEN_FILES;
   case OS_OBJECT_TYPE_OS_DIR:      return OS_MAX_NUM_OPEN_DIRS;
   case OS_OBJECT_TYPE_OS_TIMEBASE: return OS_MAX_TIMEBASES;
   case OS_OBJECT_TYPE_OS_TIMECB:   return OS_MAX_TIMERS;
   case OS_OBJECT_TYPE_OS_MODULE:   return OS_MAX_MODULES;
   case OS_OBJECT_TYPE_OS_FILESYS:  return OS_MAX_FILE_SYSTEMS;
   case OS_OBJECT_TYPE_OS_CONSOLE:  return OS_MAX_CONSOLES;
   default:                         return 0;
   }
} /* end OS_GetMaxForObjectType */


/*----------------------------------------------------------------
 *
 * Function: OS_GetBaseForObjectType
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/
uint32 OS_GetBaseForObjectType(uint32 idtype)
{
   switch(idtype)
   {
   case OS_OBJECT_TYPE_OS_TASK:     return OS_TASK_BASE;
   case OS_OBJECT_TYPE_OS_QUEUE:    return OS_QUEUE_BASE;
   case OS_OBJECT_TYPE_OS_BINSEM:   return OS_BINSEM_BASE;
   case OS_OBJECT_TYPE_OS_COUNTSEM: return OS_COUNTSEM_BASE;
   case OS_OBJECT_TYPE_OS_MUTEX:    return OS_MUTEX_BASE;
   case OS_OBJECT_TYPE_OS_STREAM:   return OS_STREAM_BASE;
   case OS_OBJECT_TYPE_OS_DIR:      return OS_DIR_BASE;
   case OS_OBJECT_TYPE_OS_TIMEBASE: return OS_TIMEBASE_BASE;
   case OS_OBJECT_TYPE_OS_TIMECB:   return OS_TIMECB_BASE;
   case OS_OBJECT_TYPE_OS_MODULE:   return OS_MODULE_BASE;
   case OS_OBJECT_TYPE_OS_FILESYS:  return OS_FILESYS_BASE;
   case OS_OBJECT_TYPE_OS_CONSOLE:  return OS_CONSOLE_BASE;
   default:                         return 0;
   }
} /* end OS_GetBaseForObjectType */

/**************************************************************
 * LOCAL HELPER FUNCTIONS
 * (not used outside of this unit)
 **************************************************************/


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectNameMatch
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           A matching function to compare the name of the record against
 *           a reference value (which must be a const char* string).
 *
 *           This allows OS_ObjectIdFindByName() to be implemented using the
 *           generic OS_ObjectIdSearch() routine.
 *
 *  returns: true if match, false otherwise
 *
 *-----------------------------------------------------------------*/
bool OS_ObjectNameMatch(void *ref, uint32 local_id, const OS_common_record_t *obj)
{
    return (obj->name_entry != NULL &&
            strcmp((const char*)ref, obj->name_entry) == 0);
} /* end OS_ObjectNameMatch */


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdInitiateLock
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *   Initiate the locking process for the given mode and ID type, prior
 *   to looking up a specific object.
 *
 *   For any lock_mode other than OS_LOCK_MODE_NONE, this acquires the
 *   global table lock for that ID type.
 *
 *   Once the lookup operation is completed, the OS_ObjectIdConvertLock()
 *   routine should be used to convert this global lock into the actual
 *   lock type requested (lock_mode).
 *
 *-----------------------------------------------------------------*/
void OS_ObjectIdInitiateLock(OS_lock_mode_t lock_mode, uint32 idtype)
{
    if (lock_mode != OS_LOCK_MODE_NONE)
    {
        OS_Lock_Global(idtype);
    }
} /* end OS_ObjectIdInitiateLock */


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdConvertLock
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *   Selectively convert the existing lock on a given resource, depending on the lock mode.
 *
 *   For any lock_mode other than OS_LOCK_MODE_NONE, the global table lock **must**
 *   already be held prior to entering this function.  This function may or may
 *   not unlock the global table, depending on the lock_mode and state of the entry.
 *
 *   For all modes, this verifies that the reference_id passed in and the active_id
 *   within the record are a match.  If they do not match, then OS_ERR_INVALID_ID
 *   is returned.
 *
 *   If lock_mode is set to either OS_LOCK_MODE_NONE or OS_LOCK_MODE_GLOBAL,
 *   no additional operation is performed, as the existing lock (if any) is
 *   sufficient and no conversion is necessary.
 *
 *   If lock_mode is set to OS_LOCK_MODE_REFCOUNT, then this increments
 *   the reference count within the object itself and releases the table lock,
 *   so long as there is no "exclusive" request already pending.
 *
 *   If lock_mode is set to OS_LOCK_MODE_EXCLUSIVE, then this verifies
 *   that the refcount is zero, but also keeps the global lock held.
 *
 *   For EXCLUSIVE and REFCOUNT style locks, if the state is not appropriate,
 *   this may unlock the global table and re-lock it several times
 *   while waiting for the state to change.
 *
 *   Returns: OS_SUCCESS if operation was successful,
 *            or suitable error code if operation was not successful.
 *
 *   NOTE: Upon failure, the global table lock is always released for
 *         all lock modes other than OS_LOCK_MODE_NONE.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdConvertLock(OS_lock_mode_t lock_mode, uint32 idtype, osal_id_t reference_id, OS_common_record_t *obj)
{
    int32 return_code = OS_ERROR;
    uint32 exclusive_bits = 0;
    uint32 attempts = 0;

    while(true)
    {
        /* Validate the integrity of the ID.  As the "active_id" is a single
         * integer, we can do this check regardless of whether global is locked or not. */
        if (!OS_ObjectIdEqual(obj->active_id, reference_id))
        {
            /* The ID does not match, so unlock and return error.
             * This basically means the ID was stale or otherwise no longer invalid */
            return_code = OS_ERR_INVALID_ID;
            break;
        }

        /*
         * The REFCOUNT and EXCLUSIVE lock modes require additional
         * conditions on before they can be successful.
         */
        if (lock_mode == OS_LOCK_MODE_REFCOUNT)
        {
            /* As long as no exclusive request is pending, we can increment the
             * refcount and good to go. */
            if ((obj->flags & OS_OBJECT_EXCL_REQ_FLAG) == 0)
            {
                ++obj->refcount;
                return_code = OS_SUCCESS;
                break;
            }
        }
        else if (lock_mode == OS_LOCK_MODE_EXCLUSIVE)
        {
            /*
             * Set the exclusive request flag -- this will prevent anyone else from
             * incrementing the refcount while we are waiting.  However we can only
             * do this if there are no OTHER exclusive requests.
             */
            if (exclusive_bits != 0 || (obj->flags & OS_OBJECT_EXCL_REQ_FLAG) == 0)
            {
                /*
                 * As long as nothing is referencing this object, we are good to go.
                 * The global table will be left in a locked state in this case.
                 */
                if (obj->refcount == 0)
                {
                    return_code = OS_SUCCESS;
                    break;
                }

                exclusive_bits = OS_OBJECT_EXCL_REQ_FLAG;
                obj->flags |= (uint16)exclusive_bits;
            }
        }
        else
        {
            /* No fanciness required - move on. */
            return_code = OS_SUCCESS;
            break;
        }


        /*
         * If we get this far, it means there is contention for access to the object.
         *  a) we want to increment refcount but an exclusive is pending
         *  b) we want exclusive but refcount is nonzero
         *  c) we want exclusive but another exclusive is pending
         *
         * In this case we will UNLOCK the global object again so that the holder
         * can relinquish it.  We'll try again a few times before giving up hope.
         */
        ++attempts;
        if (attempts >= 5)
        {
            return_code = OS_ERR_OBJECT_IN_USE;
            break;
        }

        OS_Unlock_Global(idtype);
        OS_TaskDelay_Impl(attempts);
        OS_Lock_Global(idtype);
    }

    /*
     * Determine if the global table needs to be unlocked now.
     *
     * If lock_mode is OS_LOCK_MODE_NONE, then the table was never locked
     * to begin with, and therefore never needs to be unlocked.
     */
    if (lock_mode != OS_LOCK_MODE_NONE)
    {
        /*
         * In case any exclusive bits were set locally, unset them now
         * before the lock is (maybe) released.
         */
        obj->flags &= (uint16)~exclusive_bits;

        /*
         * If the operation failed, then we always unlock the global table.
         *
         * On a successful operation, the global is unlocked if it is a REFCOUNT
         * style lock.  For other styles (GLOBAL or EXCLUSIVE) the global lock
         * should be maintained and returned to the caller.
         */
        if (return_code != OS_SUCCESS ||
                lock_mode == OS_LOCK_MODE_REFCOUNT)
        {
            OS_Unlock_Global(idtype);
        }
    }

    return return_code;

} /* end OS_ObjectIdConvertLock */

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdSearch
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Locate an existing object using the supplied Match function.
 *           Matching object ID is stored in the object_id pointer
 *
 *           This is an internal function and no table locking is performed here.
 *           Locking must be done by the calling function.
 *
 *  returns: OS_ERR_NAME_NOT_FOUND if not found, OS_SUCCESS if match is found
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdSearch(uint32 idtype, OS_ObjectMatchFunc_t MatchFunc, void *arg, OS_common_record_t **record)
{
    int32 return_code;
    uint32 obj_count;
    uint32 local_id;
    OS_common_record_t *obj;

    return_code = OS_ERR_NAME_NOT_FOUND;
    obj = &OS_common_table[OS_GetBaseForObjectType(idtype)];
    obj_count = OS_GetMaxForObjectType(idtype);
    local_id = 0;

    while (true)
    {
        if (obj_count == 0)
        {
            obj = NULL;
            break;
        }
        --obj_count;

        if ( OS_ObjectIdDefined(obj->active_id) &&
                MatchFunc(arg, local_id, obj))
        {
            return_code = OS_SUCCESS;
            break;
        }
        ++obj;
        ++local_id;
    }

    if (record != NULL)
    {
        *record = obj;
    }

    return return_code;
} /* end OS_ObjectIdSearch */

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdFindNext
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Find the next available Object ID of the given type
 *           Searches the global name/id table for an open entry of the given type.
 *           The search will start at the location of the last-issued ID.
 *
 *           Note: This is an internal helper function and no locking is performed.
 *           The appropriate global table lock must be held prior to calling this.
 *
 *  Outputs: *record is set to point to the global entry and active_id member is set
 *           *array_index updated to the offset of the found entry (local_id)
 *
 *  returns: OS_SUCCESS if an empty location was found.
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdFindNext(uint32 idtype, uint32 *array_index, OS_common_record_t **record)
{
   uint32 max_id;
   uint32 base_id;
   uint32 local_id = 0;
   uint32 idvalue;
   uint32 i;
   int32 return_code;
   OS_common_record_t *obj = NULL;

   base_id = OS_GetBaseForObjectType(idtype);
   max_id = OS_GetMaxForObjectType(idtype);

   if (max_id == 0)
   {
       /* if the max id is zero, then this build of OSAL
        * does not include any support for that object type.
        * Return the "not implemented" to differentiate between
        * this case vs. running out of valid slots  */
       return_code = OS_ERR_NOT_IMPLEMENTED;
       idvalue = 0;
   }
   else
   {
       return_code = OS_ERR_NO_FREE_IDS;
       idvalue = OS_ObjectIdToSerialNumber_Impl(OS_objtype_state[idtype].last_id_issued);
   }

   for (i = 0; i < max_id; ++i)
   {
      local_id = (++idvalue) % max_id;
      if (idvalue >= OS_OBJECT_INDEX_MASK)
      {
          /* reset to beginning of ID space */
          idvalue = local_id;
      }
      obj = &OS_common_table[local_id + base_id];
      if (!OS_ObjectIdDefined(obj->active_id))
      {
         return_code = OS_SUCCESS;
         break;
      }
   }

   if(return_code == OS_SUCCESS)
   {
       OS_ObjectIdCompose_Impl(idtype, idvalue, &obj->active_id);

       /* Ensure any data in the record has been cleared */
       obj->name_entry = NULL;
       obj->creator = OS_TaskGetId();
       obj->refcount = 0;
   }

   if(return_code != OS_SUCCESS)
   {
       obj = NULL;
       local_id = 0;
   }

   if (array_index != NULL)
   {
       *array_index = local_id;
   }
   if (record != NULL)
   {
       *record = obj;
   }

   return return_code;
} /* end OS_ObjectIdFindNext */


/*
 *********************************************************************************
 *          OSAL INTERNAL FUNCTIONS
 *
 * These functions are invoked by other units within OSAL,
 *  but are NOT directly invoked by applications
 *********************************************************************************
 */

/*----------------------------------------------------------------
   Function: OS_Lock_Global

    Purpose: Locks the global table identified by "idtype"
 ------------------------------------------------------------------*/
void OS_Lock_Global(uint32 idtype)
{
    int32 return_code;
    osal_id_t self_task_id;
    OS_objtype_state_t *objtype;

    if (idtype < OS_OBJECT_TYPE_USER)
    {
        objtype = &OS_objtype_state[idtype];
        self_task_id = OS_TaskGetId_Impl();

        return_code = OS_Lock_Global_Impl(idtype);
        if (return_code == OS_SUCCESS)
        {
            /*
             * Track ownership of this table.  It should only be owned by one
             * task at a time, and this aids in recovery if the owning task is
             * deleted or experiences an exception causing it to not be freed.
             *
             * This is done after successfully locking, so this has exclusive access
             * to the state object.
             */
            if ( !OS_ObjectIdDefined(self_task_id) )
            {
                /*
                 * This just means the calling context is not an OSAL-created task.
                 * This is not necessarily an error, but it should be tracked.
                 * Also note that the root/initial task also does not have an ID.
                 */
                self_task_id = OS_OBJECT_ID_RESERVED; /* nonzero, but also won't alias a known task */
            }

            if ( OS_ObjectIdDefined(objtype->table_owner) )
            {
                /* this is almost certainly a bug */
                OS_DEBUG("ERROR: global %u acquired by task 0x%lx when already owned by task 0x%lx\n",
                        (unsigned int)idtype,
                        OS_ObjectIdToInteger(self_task_id),
                        OS_ObjectIdToInteger(objtype->table_owner));
            }
            else
            {
                objtype->table_owner = self_task_id;
            }
        }
    }
    else
    {
        return_code = OS_ERR_INCORRECT_OBJ_TYPE;
    }

    if (return_code != OS_SUCCESS)
    {
        OS_DEBUG("ERROR: unable to lock global %u, error=%d\n", (unsigned int)idtype, (int)return_code);
    }
}

/*----------------------------------------------------------------
   Function: OS_Unlock_Global

    Purpose: Unlocks the global table identified by "idtype"
 ------------------------------------------------------------------*/
void OS_Unlock_Global(uint32 idtype)
{
    int32 return_code;
    osal_id_t self_task_id;
    OS_objtype_state_t *objtype;

    if (idtype < OS_OBJECT_TYPE_USER)
    {
        objtype = &OS_objtype_state[idtype];
        self_task_id = OS_TaskGetId_Impl();

        /*
         * Un-track ownership of this table.  It should only be owned by one
         * task at a time, and this aids in recovery if the owning task is
         * deleted or experiences an exception causing it to not be freed.
         *
         * This is done before unlocking, while this has exclusive access
         * to the state object.
         */
        if ( !OS_ObjectIdDefined(self_task_id) )
        {
            /*
             * This just means the calling context is not an OSAL-created task.
             * This is not necessarily an error, but it should be tracked.
             * Also note that the root/initial task also does not have an ID.
             */
            self_task_id = OS_OBJECT_ID_RESERVED; /* nonzero, but also won't alias a known task */
        }

        if ( !OS_ObjectIdEqual(objtype->table_owner, self_task_id) )
        {
            /* this is almost certainly a bug */
            OS_DEBUG("ERROR: global %u released by task 0x%lx when owned by task 0x%lx\n",
                    (unsigned int)idtype,
                    OS_ObjectIdToInteger(self_task_id),
                    OS_ObjectIdToInteger(objtype->table_owner));
        }
        else
        {
            objtype->table_owner = OS_OBJECT_ID_UNDEFINED;
        }

        return_code = OS_Unlock_Global_Impl(idtype);
    }
    else
    {
        return_code = OS_ERR_INCORRECT_OBJ_TYPE;
    }

    if (return_code != OS_SUCCESS)
    {
        OS_DEBUG("ERROR: unable to unlock global %u, error=%d\n", (unsigned int)idtype, (int)return_code);
    }
}

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdFinalizeNew
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Called when the initialization of a newly-issued object ID is fully complete,
 *           to perform finalization of the object and record state.
 *
 *           If the operation_status was successful (OS_SUCCESS) then the ID is exported
 *           to the caller through the "outid" pointer.
 *
 *           If the operation_status is unsuccessful, then the temporary id in the record
 *           is cleared and an ID value of 0 is exported to the caller.
 *
 *  returns: The same operation_status value passed-in, or OS_ERR_INVALID_ID if problems
 *           were detected while validating the ID.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdFinalizeNew(int32 operation_status, OS_common_record_t *record, osal_id_t *outid)
{
    uint32 idtype = OS_ObjectIdToType_Impl(record->active_id);

    /* if operation was unsuccessful, then clear
     * the active_id field within the record, so
     * the record can be re-used later.
     *
     * Otherwise, ensure that the record_id to be
     * exported is sane (it always should be)
     */
    if (operation_status != OS_SUCCESS)
    {
        record->active_id = OS_OBJECT_ID_UNDEFINED;
    }
    else if (idtype == 0 || idtype >= OS_OBJECT_TYPE_USER)
    {
        /* should never happen - indicates a bug. */
        operation_status = OS_ERR_INVALID_ID;
        record->active_id = OS_OBJECT_ID_UNDEFINED;
    }
    else
    {
        /* success */
        OS_objtype_state[idtype].last_id_issued = record->active_id;
    }

    if (outid != NULL)
    {
        /* always write the final value to the output buffer */
        *outid = record->active_id;
    }

    /* Either way we must unlock the object type */
    OS_Unlock_Global(idtype);

    return operation_status;
} /* end OS_ObjectIdFinalizeNew */

/*----------------------------------------------------------------
   Function: OS_ObjectIdFinalizeDelete

    Purpose: Helper routine, not part of OSAL public API.
             See description in prototype
 ------------------------------------------------------------------*/
int32 OS_ObjectIdFinalizeDelete(int32 operation_status, OS_common_record_t *record)
{
    uint32 idtype = OS_ObjectIdToType_Impl(record->active_id);

    /* Clear the OSAL ID if successful - this returns the record to the pool */
    if (operation_status == OS_SUCCESS)
    {
        record->active_id = OS_OBJECT_ID_UNDEFINED;
    }

    /* Either way we must unlock the object type */
    OS_Unlock_Global(idtype);

    return operation_status;
}


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdGetBySearch
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Locate an existing object using the supplied Match function.
 *           Matching object ID is stored in the object_id pointer
 *
 *           Global locking is performed according to the lock_mode
 *           parameter.
 *
 *  returns: OS_ERR_NAME_NOT_FOUND if not found, OS_SUCCESS if match is found
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdGetBySearch(OS_lock_mode_t lock_mode, uint32 idtype, OS_ObjectMatchFunc_t MatchFunc, void *arg, OS_common_record_t **record)
{
    int32 return_code;
    OS_common_record_t *obj;

    OS_ObjectIdInitiateLock(lock_mode, idtype);

    return_code = OS_ObjectIdSearch(idtype, MatchFunc, arg, &obj);

    if (return_code == OS_SUCCESS)
    {
        /*
         * The "ConvertLock" routine will return with the global lock
         * in a state appropriate for returning to the caller, as indicated
         * by the "check_mode" parameter.
         */
        return_code = OS_ObjectIdConvertLock(lock_mode, idtype, obj->active_id, obj);
    }
    else if (lock_mode != OS_LOCK_MODE_NONE)
    {
        OS_Unlock_Global(idtype);
    }

    if (record != NULL)
    {
        *record = obj;
    }

    return return_code;
} /* end OS_ObjectIdGetBySearch */


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdGetByName
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Locate an existing object with matching name and type
 *           Matching record is stored in the record pointer
 *
 *           Global locking is performed according to the lock_mode
 *           parameter.
 *
 *  returns: OS_ERR_NAME_NOT_FOUND if not found, OS_SUCCESS if match is found
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdGetByName (OS_lock_mode_t lock_mode, uint32 idtype, const char *name, OS_common_record_t **record)
{
    return  OS_ObjectIdGetBySearch(lock_mode, idtype, OS_ObjectNameMatch, (void*)name, record);

} /* end OS_ObjectIdGetByName */

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdFindByName
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Locate an existing object with matching name and type
 *           Matching object ID is stored in the object_id pointer
 *
 *  returns: OS_ERR_NAME_NOT_FOUND if not found, OS_SUCCESS if match is found
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdFindByName (uint32 idtype, const char *name, osal_id_t *object_id)
{
    int32 return_code;
    OS_common_record_t *global;

    /*
     * As this is an internal-only function, calling it will NULL is allowed.
     * This is required by the file/dir/socket API since these DO allow multiple
     * instances of the same name.
     */
    if (name == NULL)
    {
        return OS_ERR_NAME_NOT_FOUND;
    }

    if (strlen(name) >= OS_MAX_API_NAME)
    {
        return OS_ERR_NAME_TOO_LONG;
    }

    return_code = OS_ObjectIdGetByName(OS_LOCK_MODE_GLOBAL, idtype, name, &global);
    if (return_code == OS_SUCCESS)
    {
        *object_id = global->active_id;
        OS_Unlock_Global(idtype);
    }

    return return_code;

} /* end OS_ObjectIdFindByName */



/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdGetById
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Gets the resource record pointer and index associated with the given resource ID.
 *           If successful, this returns with the item locked according to "lock_mode".
 *
 *           IMPORTANT: when this function returns OS_SUCCESS with lock_mode something
 *           other than NONE, then the caller must take appropriate action to UN lock
 *           after completing the respective operation.  The OS_ObjectIdRelease()
 *           function may be used to release the lock appropriately for the lock_mode.
 *
 *           If this returns something other than OS_SUCCESS then the global is NOT locked.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdGetById(OS_lock_mode_t lock_mode, uint32 idtype, osal_id_t id, uint32 *array_index, OS_common_record_t **record)
{
   int32 return_code;

   if (OS_SharedGlobalVars.Initialized == false)
   {
       return OS_ERROR;
   }

   /*
    * Special case to allow only OS_LOCK_MODE_EXCLUSIVE during shutdowns
    * (This is the lock mode used to delete objects)
    */
   if (OS_SharedGlobalVars.ShutdownFlag == OS_SHUTDOWN_MAGIC_NUMBER &&
           lock_mode != OS_LOCK_MODE_EXCLUSIVE)
   {
       return OS_ERR_INCORRECT_OBJ_STATE;
   }


   return_code = OS_ObjectIdToArrayIndex(idtype, id, array_index);
   if (return_code != OS_SUCCESS)
   {
       return return_code;
   }


   *record = &OS_common_table[*array_index + OS_GetBaseForObjectType(idtype)];

   OS_ObjectIdInitiateLock(lock_mode, idtype);

   /*
    * The "ConvertLock" routine will return with the global lock
    * in a state appropriate for returning to the caller, as indicated
    * by the "check_mode" paramter.
    *
    * Note If this operation fails, then it always unlocks the global for
    * all check_mode's other than NONE.
    */
   return_code = OS_ObjectIdConvertLock(lock_mode, idtype, id, *record);

   return return_code;
} /* end OS_ObjectIdGetById */


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdRefcountDecr
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Decrement the reference count on the resource record, which must have been
 *           acquired (incremented) by the caller prior to this.
 *
 *  returns: OS_SUCCESS if decremented successfully.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdRefcountDecr(OS_common_record_t *record)
{
   int32 return_code;
   uint32 idtype = OS_ObjectIdToType_Impl(record->active_id);

   if (idtype == 0 || !OS_ObjectIdDefined(record->active_id))
   {
      return_code = OS_ERR_INVALID_ID;
   }
   else
   {
      OS_Lock_Global(idtype);

      if (record->refcount > 0)
      {
         --record->refcount;
         return_code = OS_SUCCESS;
      }
      else
      {
         return_code = OS_ERR_INCORRECT_OBJ_STATE;
      }

      OS_Unlock_Global(idtype);
   }

   return return_code;
} /* end OS_ObjectIdRefcountDecr */

/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdAllocateNew
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Locks the global table for the indicated ID type and allocates a
 *           new object of the given type with the given name.
 *
 *   Inputs: last_alloc_id represents the previously issued ID of this type.
 *              (The search for a free entry will start here +1 to avoid repeats).
 *
 *  Outputs: *record is set to point to the global entry and active_id member is set
 *
 *  returns: OS_SUCCESS if a NEW object was allocated and the table remains locked.
 *
 *  IMPORTANT: The global table is remains in a locked state if this returns OS_SUCCESS,
 *             so that additional initialization can be performed in an atomic manner.
 *
 *             If this fails for any reason (i.e. a duplicate name or no free slots)
 *             then the global table is unlocked inside this function prior to
 *             returning to the caller.
 *
 *             If OS_SUCCESS is returned, then the global lock MUST be either unlocked
 *             or converted to a different style lock (see OS_ObjectIdConvertLock) once
 *             the initialization of the new object is completed.
 *
 *             For any return code other than OS_SUCCESS, the caller must NOT
 *             manipulate the global lock at all.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdAllocateNew(uint32 idtype, const char *name, uint32 *array_index, OS_common_record_t **record)
{
   int32 return_code;

   if (OS_SharedGlobalVars.Initialized == false ||
         OS_SharedGlobalVars.ShutdownFlag == OS_SHUTDOWN_MAGIC_NUMBER)
   {
       return OS_ERROR;
   }

   if (idtype >= OS_OBJECT_TYPE_USER)
   {
       return OS_ERR_INCORRECT_OBJ_TYPE;
   }

   OS_Lock_Global(idtype);

   /*
    * Check if an object of the same name already exits.
    * If so, a new object cannot be allocated.
    */
   if (name != NULL)
   {
       return_code = OS_ObjectIdSearch(idtype, OS_ObjectNameMatch, (void*)name, record);
   }
   else
   {
       return_code = OS_ERR_NAME_NOT_FOUND;
   }

   if (return_code == OS_SUCCESS)
   {
      return_code = OS_ERR_NAME_TAKEN;
   }
   else
   {
      return_code = OS_ObjectIdFindNext(idtype, array_index, record);
   }

   /* If allocation failed for any reason, unlock the global.
    * otherwise the global should stay locked so remaining initialization can be done */
   if (return_code != OS_SUCCESS)
   {
      OS_Unlock_Global(idtype);
   }

   return return_code;
} /* end OS_ObjectIdAllocateNew */

/*
 *********************************************************************************
 *          PUBLIC API (these functions may be called externally)
 *********************************************************************************
 */

/*----------------------------------------------------------------
 *
 * Function: OS_ConvertToArrayIndex
 *
 *  Purpose: Implemented per public OSAL API
 *           See description in API and header file for detail
 *
 *-----------------------------------------------------------------*/
int32 OS_ConvertToArrayIndex(osal_id_t object_id, uint32 *ArrayIndex)
{
    /* just pass to the generic internal conversion routine */
    return OS_ObjectIdToArrayIndex(OS_OBJECT_TYPE_UNDEFINED, object_id, ArrayIndex);
} /* end OS_ConvertToArrayIndex */


/*----------------------------------------------------------------
 *
 * Function: OS_ForEachObject
 *
 *  Purpose: Implemented per public OSAL API
 *           See description in API and header file for detail
 *
 *-----------------------------------------------------------------*/
void OS_ForEachObject (osal_id_t creator_id, OS_ArgCallback_t callback_ptr, void *callback_arg)
{
    uint32 idtype;

    for (idtype = 0; idtype < OS_OBJECT_TYPE_USER; ++idtype)
    {
        OS_ForEachObjectOfType(idtype, creator_id, callback_ptr, callback_arg);
    }
} /* end OS_ForEachObject */

/*-----------------------------------------------------------------
 *
 * Function: OS_ForEachObjectOfType
 *
 *  Purpose: Implemented per public OSAL API
 *           See description in API and header file for detail
 *
 *-----------------------------------------------------------------*/
void OS_ForEachObjectOfType     (uint32 idtype, osal_id_t creator_id, OS_ArgCallback_t callback_ptr, void *callback_arg)
{
    uint32 obj_index;
    uint32 obj_max;
    osal_id_t obj_id;

    obj_max = OS_GetMaxForObjectType(idtype);
    if (obj_max > 0)
    {
        obj_index = OS_GetBaseForObjectType(idtype);
        OS_Lock_Global(idtype);
        while (obj_max > 0)
        {
            /*
             * Check if the obj_id is both valid and matches
             * the specified creator_id
             */
            obj_id = OS_common_table[obj_index].active_id;
            if (OS_ObjectIdDefined(obj_id) &&
                    !OS_ObjectIdEqual(creator_id, OS_OBJECT_CREATOR_ANY) &&
                    !OS_ObjectIdEqual(OS_common_table[obj_index].creator, creator_id))
            {
                /* valid object but not a creator match -
                 * skip the callback for this object */
                obj_id = OS_OBJECT_ID_UNDEFINED;
            }

            if (OS_ObjectIdDefined(obj_id))
            {
                /*
                 * Invoke Callback for the object, which must be done
                 * while the global table is unlocked.
                 *
                 * Note this means by the time the callback is done,
                 * the object could have been deleted by another task.
                 *
                 * But this must not invoke a callback with a locked table,
                 * as the callback function might call other OSAL functions,
                 * which could deadlock.
                 */
                OS_Unlock_Global(idtype);
                (*callback_ptr)(obj_id, callback_arg);
                OS_Lock_Global(idtype);
            }

            ++obj_index;
            --obj_max;
        }
        OS_Unlock_Global(idtype);
    }
} /* end OS_ForEachObjectOfType */

/*----------------------------------------------------------------
 *
 * Function: OS_IdentifyObject
 *
 *  Purpose: Implemented per public OSAL API
 *           See description in API and header file for detail
 *
 *-----------------------------------------------------------------*/
uint32 OS_IdentifyObject       (osal_id_t object_id)
{
    return OS_ObjectIdToType_Impl(object_id);
} /* end OS_IdentifyObject */

/*----------------------------------------------------------------
 *
 * Function: OS_GetResourceName
 *
 *  Purpose: Implemented per public OSAL API
 *           See description in API and header file for detail
 *
 *-----------------------------------------------------------------*/
int32 OS_GetResourceName(osal_id_t object_id, char *buffer, uint32 buffer_size)
{
    uint32 idtype;
    OS_common_record_t *record;
    int32 return_code;
    size_t name_len;
    uint32 local_id;

    /* sanity check the passed-in buffer and size */
    if (buffer == NULL || buffer_size == 0)
    {
        return OS_INVALID_POINTER;
    }

    /*
     * Initially set the output string to empty.
     * This avoids undefined behavior in case the function fails
     * and the caller does not check the return code.
     */
    buffer[0] = 0;

    idtype = OS_ObjectIdToType_Impl(object_id);
    return_code = OS_ObjectIdGetById(OS_LOCK_MODE_GLOBAL, idtype, object_id, &local_id, &record);
    if (return_code == OS_SUCCESS)
    {
        if (record->name_entry != NULL)
        {
            name_len = strlen(record->name_entry);
            if (buffer_size <= name_len)
            {
                /* indicates the name does not fit into supplied buffer */
                return_code = OS_ERR_NAME_TOO_LONG;
                name_len = buffer_size - 1;
            }
            memcpy(buffer, record->name_entry, name_len);
            buffer[name_len] = 0;
        }
        OS_Unlock_Global(idtype);
    }

    return return_code;
} /* end OS_GetResourceName */


/*----------------------------------------------------------------
 *
 * Function: OS_ObjectIdToArrayIndex
 *
 *  Purpose: Convert an object ID (which must be of the given type) to a number suitable
 *           for use as an array index.  The array index will be in the range of:
 *            0 <= ArrayIndex < OS_MAX_<OBJTYPE>
 *
 *            If the passed-in ID type is OS_OBJECT_TYPE_UNDEFINED, then any type
 *            is allowed.
 *
 *  returns: If the passed-in ID is not of the proper type, OS_ERROR is returned
 *           Otherwise OS_SUCCESS is returned.
 *
 *-----------------------------------------------------------------*/
int32 OS_ObjectIdToArrayIndex(uint32 idtype, osal_id_t id, uint32 *ArrayIndex)
{
   uint32 max_id;
   uint32 obj_index;
   uint32 actual_type;
   int32 return_code;

   obj_index = OS_ObjectIdToSerialNumber_Impl(id);
   actual_type = OS_ObjectIdToType_Impl(id);

   /*
    * If requested by the caller, enforce that the ID is of the correct type.
    * If the caller passed OS_OBJECT_TYPE_UNDEFINED, then anything is allowed.
    */
   if (idtype != OS_OBJECT_TYPE_UNDEFINED && actual_type != idtype)
   {
       return_code = OS_ERR_INVALID_ID;
   }
   else
   {
       max_id = OS_GetMaxForObjectType(actual_type);
       if (max_id == 0)
       {
           return_code = OS_ERR_INVALID_ID;
       }
       else
       {
           return_code = OS_SUCCESS;
           *ArrayIndex = obj_index % max_id;
       }
   }

   return return_code;
} /* end OS_ObjectIdToArrayIndex */


