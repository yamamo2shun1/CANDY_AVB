/*
 * FILENAME: GENLIST.H
 *
 * Copyright 1999-2002 By InterNiche Technologies Inc. All rights reserved
 *
 * MODULE: MISCLIB
 *
 * ROUTINES:
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* 
 * GENERIC Implementation of a list. 
 *
 * 1999      - Implemented for use with SNMPv3 tables -AT-
 * 2/21/2002 - Added niche_add_sorted() and moved from snmpv3 to h -AT-
 */


#ifndef GENLIST_H    /* Make sure the header file is included only once */
#define  GENLIST_H            1

#include "ipport.h"

/* Generic list is used by SNMPv3 code and IPFILTER code. */
#if (defined(INCLUDE_SNMPV3) || defined(USE_IPFILTER) || defined(INICHE_SYSLOG))
#define USE_GENLIST     1
#endif

#define  GEN_MAX_ARRAY  10

#ifndef GEN_ALLOC
#define GEN_ALLOC(size) npalloc(size)
#endif
#ifndef GEN_FREE
#define GEN_FREE(ptr)   npfree(ptr)    
#endif

#ifndef MEMCMP
#define MEMCMP          memcmp
#endif
#ifndef MEMCPY
#define MEMCPY          memcpy
#endif

#ifndef SUCCESS
#define SUCCESS         0
#endif
#ifndef FAILURE
#define FAILURE         1
#endif


#ifndef MAX_NAME_LEN      /* usually defined in snmpport.h */
#define MAX_NAME_LEN    64  /* max number of subid's in a objid */
#endif

/* NicheElement had two members. One is a pointer to the DATA and the 
 * other is a pointer to the next element. If there is no next 
 * element, the pointer is NULL. GEN_STRUCT can be anything. 
 * Following are some thoughts on the way things are declared and 
 * intended. 
 * 1. If there is only one list, GEN_STRUCT can be 
 *    explicitly named to the STRUCT/CLASS 
 * 2. If there are multiple lists, then GEN_STRUCT can be "void*" 
 * and casting can be done whenever we are using it 
 * 3. If we need a CLASS implementation, all the functions can be 
 *   made as interfaces of the CLASS. So we just 
 *   need a class wrapper. Further, it is assumed that GEN_STRUCT has 
 *   two members defined as follows 
 *      "long id;" 
 *      "char name[MAX_NAME_LEN];" 
 * The list can be a singly indexed list (that 
 * is all elements can be uniquely identified by their IDs). Or it 
 * can be doubly indexed list (all elements can be uniquely 
 * identified by their ID and name combined). Lookup and deletion 
 * from the list has been provided for this use. 
 */
struct TemplateStruct
{
   long     id;
   char     name[MAX_NAME_LEN];
};

typedef struct TemplateStruct * GEN_STRUCT ;

struct NicheElement
{
   GEN_STRUCT  p_data;           /* Pointer to element/data */       
   struct     NicheElement *  next; /* Pointer to next data element */
};

typedef struct NicheElement * NICHE_ELE;


struct NicheList
{
   NICHE_ELE   head  ;     /* First element of the list */
   int         len_of_element;/* Len   of the   struct representing  element/data*/
};

typedef struct NicheList * NICHELIST;


int        niche_list_constructor  (NICHELIST list, int len_of_ele);
int        niche_list_destructor   (NICHELIST list);
int        niche_add               (NICHELIST list, GEN_STRUCT ptr_data);
int        niche_add_sorted        (NICHELIST list, GEN_STRUCT ptr_data);
int        niche_add_id_and_name   (NICHELIST list, long id, char * name);
int        niche_del               (NICHELIST list, GEN_STRUCT ptr_data);
int        niche_del_id            (NICHELIST list, long id);
int        niche_del_name          (NICHELIST list, char * name);
int        niche_del_id_and_name   (NICHELIST list, long id, char * name);
GEN_STRUCT niche_lookup_id         (NICHELIST list, long id);
GEN_STRUCT niche_lookup_name       (NICHELIST list, char *name);
GEN_STRUCT niche_lookup_id_and_name(NICHELIST list, long id, char *name);
int        niche_lookup_multi_match(NICHELIST list, long id, char * name, 
              GEN_STRUCT matches[]);
int        niche_list_show         (NICHELIST list);
int        niche_list_len          (NICHELIST list); 
GEN_STRUCT niche_list_getat        (NICHELIST list,int index); 
int        niche_element_show      (GEN_STRUCT ptr_data);



#define  NICHE_ERRBASE                    2000

#define  NICHE_NO_MEM                     (NICHE_ERRBASE+1)
#define  NICHE_NO_MEM_FOR_DATA            (NICHE_ERRBASE+2)
#define  NICHE_ADD_NOT_ENOUGH_MEMORY      (NICHE_ERRBASE+3)
#define  NICHE_DEL_LIST_EMPTY             (NICHE_ERRBASE+4)
#define  NICHE_DEL_NOT_FOUND              (NICHE_ERRBASE+5)
#define  NICHE_DEL_NOT_ENOUGH_MEMORY      (NICHE_ERRBASE+6)
#define  NICHE_LISTPTR_INVALID            (NICHE_ERRBASE+7)
#define  NICHE_DUP_ENTRY                  (NICHE_ERRBASE+8)


#endif   /*  GENLIST_H */

