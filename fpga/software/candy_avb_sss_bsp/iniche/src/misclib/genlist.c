/*
 * FILENAME: genlist.c
 *
 * Copyright 1999- 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * GENERIC Implementation of a list. Used by SNMPv3, IPFILTER.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: niche_list_constructor(), niche_list_destructor(), 
 * ROUTINES: niche_add(), niche_add_sorted(), niche_add_id_and_name(), 
 * ROUTINES: niche_del(), niche_del_id(), 
 * ROUTINES: niche_del_name(), niche_del_id_and_name(), 
 * ROUTINES: niche_lookup_id(), niche_lookup_name(), 
 * ROUTINES: niche_lookup_id_and_name(), niche_list_show(), niche_list_len(), 
 * ROUTINES: niche_list_getat(), niche_element_show(), 
 * ROUTINES: niche_lookup_multi_match(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* 
 * 1999      - Implemented for use with SNMPv3 tables -AT-
 * 2/21/2002 - Added niche_add_sorted() and moved from snmpv3 to misclib -AT-
 */


#include "genlist.h"

#ifdef USE_GENLIST   /* If USE_GENLIST is supported, compile this file */

/* Implementation notes 
 * 1. Allocation done by niche_add*() routines for new elements
 * 2. Free() done by niche_del*() routines for elements to be deleted.
 * 3. Each element is assumed to have a "long id" and "char name[]" as
 *    members
 * 4. niche_list_destructor() deletes all elements from the list.
 *
 * Usability notes 
 *
 * WHEN CAN THIS GENERIC LIST BE USED ???
 *
 * It is perfect for the following requirements. The implementation has
 * two fields ( number, name ) , and in places where only one of them is
 * used for indexing, the other field can be used to store some other
 * value, or left blank. If the "name" field is not to be  used, then
 * the size of "char array" can be reduced to 1 char to save memory.
 *
 * 1. For a list having number based indexing
 * 2. For a list having name based indexing
 * 3. For a doubly indexed list based on number,name.
 * In all the above cases, it is assumed that all entries can be 
 * uniquely identfied based on the index. All the functions for
 * list manipulation can be used as such. And the function
 * "niche_lookup_multi_match()" has no significance in this context,
 * and should not be used.
 *
 * 4. For a list having more than 2 indices.
 * The addition and deletion functions remain the same. The LOOKUP problem
 * in this case is that the "number,name" based dual index can't identify
 * a unique entry. Hence a special function has been provided. Its
 * "niche_lookup_multi_match()". It is an extenstion of the function
 * "niche_lookup_id_and_name()". It accepts one more argument, which is
 * a array of pointers. All the matched entries are returned via this array.
 * This function returns the "number of entries in the array".
 * 
 *
 *
 * 1. call niche_list_constructor() to init a new list. For eg.
 *    struct AppElement
 *    {
 *       long id;
 *       char   name[MAX_NAME_LEN];
 *    };
 *
 *    struct NicheList app_list;
 *    NICHELIST p_app_list = &app_list;
 *
 *    niche_list_constructor(p_app_list,sizeof(AppElement));
 *
 * 2. Call niche_add*() to add elements to the list. For eg.
 *    struct AppElement ele1 = { 1000, "router" };   
 *    niche_add(p_app_list,&ele1);
 *    niche_add_id_and_name(p_app_list,1000,"bridge");
 *    niche_add_id_and_name(p_app_list,1000,"gateway");
 *    niche_add_id_and_name(p_app_list,1001,"gateway");
 * 3. Use niche_lookup*() to find elements in the list.
 *    struct AppElement *ele2, *ele3;
 *    ele2=niche_lookup_id(p_app_list,1000);  // Looks for first match 
 *    if ( ele2 == NULL )
 *    {
 *       // not found 
 *    }
 *    ele3=niche_lookup_id_and_name(p_app_list,1000,"router");  
 *    if ( ele3 == NULL )
 *    {
 *       // not found 
 *    }
 * 4. Use niche_del*() to delete elements from the list. For eg.
 *    niche_del_id(p_app_list1000);  // Delete all elements with "id=1000"
 *    niche_del_id_and_name(p_app_list1001,"gateway");
 * 5. Use niche_list_destructor() delete all elements from the list.
 *    niche_list_destructor(p_app_list);
 */

#ifndef V3_STATIC_TABLES

/* FUNCTION: niche_list_constructor()
 * 
 * Initialize a list so that it can be used for storing data.
 *
 * PARAM1: NICHELIST list - List to be initialized
 * PARAM2: int len_of_ele - Sizeof  each data element
 *
 * REMARKS: The list should be DEFINED/ALLOCATED in the calling program.
 * This function just does initialization.
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_list_constructor(NICHELIST list, int len_of_ele)
{
   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   list->len_of_element =  len_of_ele  ;
   list->head  =  NULL  ;
   return   SUCCESS;
}


/* FUNCTION: niche_list_destructor()
 * 
 * Cleanup a list. That is remove all elements from the list.
 *
 * PARAM1: NICHELIST list - List to be cleaned up
 *
 * REMARKS: The list should be FREE'ED in the calling program.
 * This function deletes all the elements in the list.
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_list_destructor(NICHELIST list)
{
   NICHE_ELE   tmp;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=list->head;
   while ( tmp )
   {
      list->head=tmp->next;
      GEN_FREE(tmp->p_data);
      GEN_FREE(tmp);
      tmp=list->head;
   }
   return   SUCCESS;
}


/* FUNCTION: niche_add()
 * 
 * Add an element to the list.
 *
 * PARAM1: NICHELIST list
 * Pointer to the list in reference. (IN/OUT)
 * PARAM2: GEN_STRUCT ptr_data
 * Pointer to the element (containing data) to be put. (IN)
 *
 * REMARKS: Nothing is done to the "element". Memory is allocated to
 * store a new element, and then the new element is popullated
 * and added to the list.
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_add(NICHELIST list, GEN_STRUCT ptr_data)
{
   NICHE_ELE   tmp;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   /* Allocate memory for the new element */
   tmp   = (NICHE_ELE) GEN_ALLOC(sizeof(struct NicheElement));
   if ( tmp == NULL )
   {
      return   NICHE_NO_MEM   ;
   }

   /* Allocate memory for the DATA to be stored in the element */

   tmp->p_data = (GEN_STRUCT) GEN_ALLOC(list->len_of_element);
   if ( tmp->p_data == NULL )
   {
      GEN_FREE(tmp);
      return   NICHE_NO_MEM_FOR_DATA;
   }

   MEMCPY(tmp->p_data, ptr_data,list->len_of_element);   /* Fill up the data in it */
   tmp->next   =  NULL  ;

   /* Now add to the list */
   if ( list->head == NULL )  /* List is empty */
   {
      list->head  =  tmp   ;        /* add as the first element */
   }
   else
   {
      /* If the element is in the list, then don't add a duplicate */
      NICHE_ELE   ind_tmp;
      ind_tmp  =  list->head  ;
      while ( ind_tmp )
      {
         if ( MEMCMP(tmp->p_data, ind_tmp->p_data,list->len_of_element) == 0 )
         {
            GEN_FREE(tmp->p_data);
            GEN_FREE(tmp);
            return   NICHE_DUP_ENTRY;
         }
         ind_tmp=ind_tmp->next;
      }

      tmp->next   =  list->head;
      list->head  =  tmp;
   }  
   return   SUCCESS;
}

/* FUNCTION: niche_add_sorted()
 * 
 * Add an element to the list such that the list remains sorted by "id".
 *
 * PARAM1: NICHELIST list
 * Pointer to the list in reference. (IN/OUT)
 * PARAM2: GEN_STRUCT ptr_data
 * Pointer to the element (containing data) to be put. (IN)
 *
 * REMARKS: Nothing is done to the "element". Memory is allocated to
 * store a new element, and then the new element is popullated
 * and added to the list.
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_add_sorted(NICHELIST list, GEN_STRUCT ptr_data)
{
   NICHE_ELE newele;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   /* Allocate memory for the new element */
   newele = (NICHE_ELE) GEN_ALLOC(sizeof(struct NicheElement));
   if ( newele == NULL )
   {
      return   NICHE_NO_MEM   ;
   }

   /* Allocate memory for the DATA to be stored in the element */

   newele->p_data = (GEN_STRUCT) GEN_ALLOC(list->len_of_element);
   if ( newele->p_data == NULL )
   {
      GEN_FREE(newele);
      return   NICHE_NO_MEM_FOR_DATA;
   }

   MEMCPY(newele->p_data, ptr_data,list->len_of_element);   /* Fill up the data in it */
   newele->next   =  NULL  ;

   /* Now add to the list */
   if ( list->head == NULL )  /* List is empty */
   {
      list->head  =  newele   ;        /* add as the first element */
   }
   else if (list->head->p_data->id > newele->p_data->id)
   {
      newele->next = list->head;
      list->head = newele   ;        /* add as the first element */
   }
   else
   {
      /* If the element is in the list, then don't add a duplicate */
      NICHE_ELE tmpent;
      NICHE_ELE saveent;
      saveent = list->head  ;
      tmpent = saveent->next;
      while ( tmpent )
      {
         if (tmpent->p_data->id == newele->p_data->id)
         {
            /* Duplicate entry !!! Keep the new one */
            GEN_FREE(tmpent->p_data);
            tmpent->p_data = newele->p_data ;
            GEN_FREE(newele);
            return   NICHE_DUP_ENTRY;
         }
         if (tmpent->p_data->id > newele->p_data->id)
         {
            /* Insert the new entry before tmpent.  
             * That is between saveent and tmpent. */
            break;
         }
         /* update pointers to check the next item in the list */
         saveent=tmpent;
         tmpent=tmpent->next;
      }
      /* Insert the new entry after saveent */
      newele->next=saveent->next;
      saveent->next = newele;

   }  
   return   SUCCESS;
}

/* later: check for duplicate entries */

/* FUNCTION: niche_add_id_and_name()
 * 
 * Add an element to the list.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long id - Id of the new element (IN)
 * PARAM3: char *name - Name of the new element (IN)
 *
 * REMARKS: Nothing is done to the "element". Memory is allocated to
 * store a new element, and then the new element is popullated
 * and added to the list.
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_add_id_and_name(NICHELIST list, long id, char * name)
{

   GEN_STRUCT  tmp_ele;
   int         ret_code;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp_ele  = (GEN_STRUCT) GEN_ALLOC (list->len_of_element);
   if ( tmp_ele == NULL )
   {
      ret_code =  NICHE_ADD_NOT_ENOUGH_MEMORY;
   }
   else
   {
      tmp_ele->id =  id;
      strcpy(tmp_ele->name, name);
      ret_code = niche_add(list, tmp_ele);
      GEN_FREE(tmp_ele);
   }

   return   ret_code;
}



/* FUNCTION: niche_del()
 * 
 * Delete an element from the list.  This should do an exact 
 * match [of DATA] and delete the entry 
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: GEN_STRUCT ptr_data - Element to be deleted(IN)
 *
 * REMARKS: Memory is FREE'ED to for the element, and then the element 
 * is removed from the list
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_del(NICHELIST list, GEN_STRUCT ptr_data)
{
   NICHE_ELE   tmp,prev;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=prev=list->head;

   /* Check is list is empty */
   if ( tmp == NULL )
   {
      return   NICHE_DEL_LIST_EMPTY ;
   }

   /* Check if the element is the first one in the list */
   /* compare all the contents */
   if (  MEMCMP ( tmp->p_data , ptr_data, list->len_of_element ) == 0 )
   {
      /* match found */
      list->head=tmp->next;
      GEN_FREE(tmp->p_data);
      GEN_FREE(tmp);
      return   SUCCESS;
   }

   tmp=prev->next;

   while ( tmp )
   {
      if (  MEMCMP ( tmp->p_data , ptr_data, list->len_of_element ) == 0 )
      {
         /* match found */
         prev->next=tmp->next;
         GEN_FREE(tmp->p_data);
         GEN_FREE(tmp);
         return   SUCCESS;
      }
      else
      {
         prev=tmp;
         tmp=tmp->next;
      }
   };

   return   NICHE_DEL_NOT_FOUND;
}


/* FUNCTION: niche_del_id()
 * 
 * Delete an element from the list.
 * Delete all entries where "id" is matched
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long    id - ID of the element (IN)
 *
 * REMARKS: Memory is FREE'ED to for the element, and then the element 
 * is removed from the list
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_del_id(NICHELIST list, long    id)
{
   NICHE_ELE   tmp,prev;
   int   del_cnt=0;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=prev=list->head;

   /* Check if list is empty */
   if ( tmp == NULL )
   {
      return   NICHE_DEL_LIST_EMPTY ;
   }

   /* Delete all the HEAD elements that match our condition */
   do
   {
      /* Check if the element is the first one in the list */
      if ( tmp->p_data->id == id ) 
      {
         /* match found */
         list->head=tmp->next;
         GEN_FREE(tmp->p_data);
         GEN_FREE(tmp);
         del_cnt++;
      }
      else
      {
         /* Match failed */
         break;
      }
      tmp=prev=list->head;
   }  while (list->head);

      tmp=prev->next;

   while ( tmp )
   {
      if ( tmp->p_data->id == id )
      {
         /* match found */
         prev->next=tmp->next;
         GEN_FREE(tmp->p_data);
         GEN_FREE(tmp); 
         tmp=prev->next;      /* point to the next element */
         del_cnt++;
      }
      else
      {
         prev=tmp;
         tmp=tmp->next;
      }
   };

   if ( del_cnt == 0 )
   {
      return   NICHE_DEL_NOT_FOUND;
   }
   else
   {
      return   del_cnt;
   }
}


/* FUNCTION: niche_del_name()
 * 
 * Delete an element from the list.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: char *name - Name of the element (IN)
 *
 * REMARKS: Memory is FREE'ED to for the element, and then the element 
 * is removed from the list
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_del_name(NICHELIST list, char * name)
{
   NICHE_ELE   tmp,prev;
   int   del_cnt=0;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=prev=list->head;

   /* Check if list is empty */
   if ( tmp == NULL )
   {
      return   NICHE_DEL_LIST_EMPTY ;
   }

   /* Delete all the HEAD elements that match our condition */
   do
   {
      /* Check if the element is the first one in the list */
      if ( strcmp(tmp->p_data->name, name) == 0 )
      {
         /* match found */
         list->head=tmp->next;
         GEN_FREE(tmp->p_data);
         GEN_FREE(tmp);
         del_cnt++;
      }
      else
      {
         /* Match failed */
         break;
      }
      tmp=prev=list->head;
   }  while (list->head);

      tmp=prev->next;

   while ( tmp )
   {
      if ( strcmp(tmp->p_data->name, name) == 0 ) 
      {
         /* match found */
         prev->next=tmp->next;
         GEN_FREE(tmp->p_data);
         GEN_FREE(tmp);
         del_cnt++;
      }
      else
      {
         prev=tmp;
         tmp=tmp->next;
      }
   };

   if ( del_cnt == 0 )
   {
      return   NICHE_DEL_NOT_FOUND;
   }
   else
   {
      return   del_cnt;
   }
}


/* FUNCTION: niche_del_id_and_name()
 * 
 * Delete an element from the list.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long id - ID of the element (IN)
 * PARAM3: char *name - Name of the element (IN)
 *
 * REMARKS: Memory is FREE'ED to for the element, and then the element 
 * is removed from the list
 *
 * RETURNS: SUCCESS / error code
 */

int        
niche_del_id_and_name(NICHELIST list, long id, char * name)
{
   GEN_STRUCT  tmp_ele;
   int         ret_code;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp_ele  = (GEN_STRUCT) GEN_ALLOC (list->len_of_element);
   if ( tmp_ele == NULL )
   {
      ret_code =  NICHE_DEL_NOT_ENOUGH_MEMORY;
   }
   else
   {
      tmp_ele->id =  id;
      strcpy(tmp_ele->name, name);
      ret_code = niche_del(list, tmp_ele);
      GEN_FREE(tmp_ele);
   }

   return   ret_code;
}

#endif /* ifndef of V3_STATIC_TABLES */

/* FUNCTION: niche_lookup_id()
 * 
 * Lookup for the first element which has this ID.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long    id - ID of the element (IN)
 *
 * RETURNS: Pointer to the element or NULL (if not found)
 */

GEN_STRUCT  
niche_lookup_id(NICHELIST list, long    id)
{
   NICHE_ELE   tmp;

   if ( list == NULL )
      return   NULL;

   tmp=list->head;
   while ( tmp )
   {
      if (tmp->p_data->id == id )
      {
         return (GEN_STRUCT)(tmp->p_data);
      }
      else
      {
         tmp=tmp->next;
      }
   }

   return   NULL;
}


/* FUNCTION: niche_lookup_name()
 * 
 * Lookup for the first element which has this name.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: char *name - name of the element (IN)
 *
 * RETURNS: Pointer to the element or NULL (if not found)
 */

GEN_STRUCT  
niche_lookup_name(NICHELIST list, char * name)
{
   NICHE_ELE   tmp;

   if ( list == NULL )
      return   NULL;

   tmp=list->head;
   while ( tmp )
   {
      if (strcmp(tmp->p_data->name, name) == 0 )
      {
         return (GEN_STRUCT)(tmp->p_data);
      }
      else
      {
         tmp=tmp->next;
      }
   }

   return   NULL;
}


/* FUNCTION: niche_lookup_id_and_name()
 * 
 * Lookup for the first element which has this ID,name.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long    id - ID of the element (IN)
 * PARAM3: char *name - name of the element (IN)
 *
 * RETURNS: Pointer to the element or NULL (if not found)
 */

GEN_STRUCT  
niche_lookup_id_and_name(NICHELIST list, long    id, char * name)
{
   NICHE_ELE   tmp;

   if ( list == NULL )
      return   NULL;

   tmp=list->head;
   while ( tmp )
   {
      if ( ( tmp->p_data->id == id ) &&
( strcmp(tmp->p_data->name, name) == 0 ) )
      {
         return (GEN_STRUCT)(tmp->p_data);
      }
      else
      {
         tmp=tmp->next;
      }
   }

   return   NULL;
}


/* FUNCTION: niche_list_show()
 * 
 * List all elements in the list
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 *
 * RETURNS: SUCCESS / error code
 */

int niche_list_show(NICHELIST list)
{
   NICHE_ELE   tmp;
   int   cnt=0;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=list->head;
   while ( tmp )
   {
      niche_element_show(tmp->p_data);
      tmp=tmp->next;
      cnt++;
   }

   if ( cnt == 0 )
      printf("List is empty.\n");
   else
      printf("List length =%d.\n", cnt);

   return   SUCCESS;
}


/* FUNCTION: niche_list_len()
 * 
 * 
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 *
 * RETURNS: SUCCESS / error code
 */

int niche_list_len(NICHELIST list)
{
   NICHE_ELE   tmp;
   int   cnt=0;

   if ( list == NULL )
      return   NICHE_LISTPTR_INVALID;

   tmp=list->head;
   while ( tmp )
   {
      cnt++;
      tmp=tmp->next;
   }

   return   cnt;
}


/* FUNCTION: niche_list_getat()
 * 
 * 
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: int index - Position of the item to be found. (0 based)
 *
 * RETURNS: Pointer to data (element at INDEX) or NULL
 */

GEN_STRUCT niche_list_getat(NICHELIST list, int index)
{
   NICHE_ELE   tmp;
   int   cnt=0;

   if ( list == NULL )
      return   NULL;

   tmp=list->head;
   while ( tmp )
   {
      if ( cnt == index )
      {
         return   tmp->p_data;
      }
      cnt++;
      tmp=tmp->next;
   }

   /* We don't have so many elements in the list */
   return   NULL;
}


/* FUNCTION: niche_element_show()
 * 
 * Display the values of an element.
 *
 * PARAM1: GEN_STRUCT ptr_data - Pointer to the element. (IN)
 *
 * RETURNS: SUCCESS / error code
 */

int niche_element_show(GEN_STRUCT ptr_data)
{
   if ( ptr_data == NULL )
      return   FAILURE;

   printf("Id= %ld, name=%s\n", ptr_data->id,ptr_data->name);

   return   SUCCESS;
}

/* FUNCTION: niche_lookup_multi_match()
 * 
 * Lookup for the first element which has this ID,name.
 * Check the whole list , and return all the entries 
 * where this condition is matched.
 *
 * PARAM1: NICHELIST list - Pointer to the list in reference. (IN/OUT)
 * PARAM2: long id - ID of the element (IN)
 * PARAM3: char *name - name of the element (IN)
 * PARAM4: GEN_STRUCT matches[]
 * An array of pointers to hold the MATCHED entries
 * Maximum size of this array is assumed  to GEN_MAX_ARRAY
 *
 * RETURNS: Number of entries matched.
 */

int  
niche_lookup_multi_match(NICHELIST list, long id, char * name, 
   GEN_STRUCT  matches[])
{
   NICHE_ELE   tmp;
   int   cnt=0;

   if ( list == NULL )
      return   0;

   if ( matches == NULL )
      return   0;

   tmp=list->head;
   while ( tmp )
   {
      if ( ( tmp->p_data->id == id ) &&
         ( strcmp(tmp->p_data->name, name) == 0 ) )
      {
         matches[cnt]=tmp->p_data   ;
         cnt++;
      }
      tmp=tmp->next;
   }

   return   cnt;
}

#endif   /* USE_GENLIST */

