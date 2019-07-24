/*
 * FILENAME: rtbtree.c
 *
 * Copyright 2001 By InterNiche Technologies Inc. All rights reserved
 *
 * IP Routing table binary tree code, heavily based on the public domain
 * "avltree.c" file. The original avltree.c is not copyrighted and is
 * explicitly assigned to the public domain. Code is based on the
 * ancient Algorithm documented by Knuth.
 * InterNiche has formatted it to our code standards doc (although it was very
 * clean to begin with) and modified it to keep the btree links directly in the
 * route table structure. This modification avoids an extra set of alloc/free
 * macros.
 *
 * MODULE: ip
 *
 * ROUTINES: avlrotleft(), avlrotright(), avlleftgrown(), 
 * ROUTINES: avlrightgrown(), avlinsert(), avlleftshrunk(), avlrightshrunk(), 
 * ROUTINES: avlfindhighest(), avlfindlowest(), avlremove(), avlaccess(), 
 * ROUTINES: avldepthfirst(), avlbreadthfirst()
 *
 * PORTABLE: yes
 */

#include "ipport.h"

#ifdef BTREE_ROUTES

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

/* FUNCTION: avlrotleft()
 * 
 *  avlrotleft: perform counterclockwise rotation
 *
 *  Parameters:
 *
 *    n           Address of a pointer to a node
 *
 * PARAM1: struct avl_node * n
 *
 * RETURNS: 
 */

void
avlrotleft(struct avl_node ** n)
{
	struct avl_node * tmp = *n;

	*n = (*n)->right;
	tmp->right = (*n)->left;
	(*n)->left = tmp;
}


/* FUNCTION: avlrotright()
 * 
 *  avlrotright: perform clockwise rotation
 *
 *  Parameters:
 *
 *    n           Address of a pointer to a node
 *
 * PARAM1: struct avlnode *n
 *
 * RETURNS: 
 */

void
avlrotright(struct avl_node ** n)
{
	struct avl_node * tmp = *n;

	*n = (*n)->left;
	tmp->left = (*n)->right;
	(*n)->right = tmp;
}



/* FUNCTION: avlleftgrown()
 * 
 *  avlleftgrown: helper function for avlinsert
 *
 *  Parameters:
 *
 *    n           Address of a pointer to a node. This node's left 
 *                subtree has just grown due to item insertion; its 
 *                "skew" flag needs adjustment, and the local tree 
 *                (the subtree of which this node is the root node) may 
 *                have become unbalanced.
 *
 *  Return values:
 *
 *    BT_OK          The local tree could be rebalanced or was balanced 
 *                from the start. The parent activations of the avlinsert 
 *                activation that called this function may assume the 
 *                entire tree is valid.
 *
 *    BT_BALANCE     The local tree was balanced, but has grown in height.
 *                Do not assume the entire tree is valid.
 *
 * PARAM1: struct avl_node *n
 *
 * RETURNS: 
 */

enum AVLRES
avlleftgrown(struct avl_node ** n)
{
   struct avl_node * rtp = *n;

   switch (rtp->skew) 
   {
   case LEFT:
      if (rtp->left->skew == LEFT) 
      {
         rtp->skew = rtp->left->skew = NONE;
         avlrotright(n);
      }
      else 
      {
         switch (rtp->left->right->skew) 
         {
         case LEFT:
            rtp->skew = RIGHT;
            rtp->left->skew = NONE;
            break;

         case RIGHT:
            rtp->skew = NONE;
            rtp->left->skew = LEFT;
            break;

         default:
            rtp->skew = NONE;
            rtp->left->skew = NONE;
         }
         rtp->left->right->skew = NONE;
         avlrotleft(&(rtp->left));
         avlrotright(n);
      }
      return BT_OK;

   case RIGHT:
      rtp->skew = NONE;
      return BT_OK;

   default:
      rtp->skew = LEFT;
      return BT_BALANCE;
   }
}



/* FUNCTION: avlrightgrown()
 * 
 *  avlrightgrown: helper function for avlinsert
 *
 *  See avlleftgrown for details.
 *
 * PARAM1: struct avl_node *n
 *
 * RETURNS: 
 */

enum AVLRES
avlrightgrown(struct avl_node ** n)
{
   struct avl_node * rtp = *n;

   switch (rtp->skew) 
   {
   case LEFT:     
      rtp->skew = NONE;
      return BT_OK;

   case RIGHT:
      if (rtp->right->skew == RIGHT) 
      { 
         rtp->skew = rtp->right->skew = NONE;
         avlrotleft(n);
      }
      else 
      {
         switch (rtp->right->left->skew) 
         {
         case RIGHT:
            rtp->skew = LEFT;
            rtp->right->skew = NONE;
            break;

         case LEFT:
            rtp->skew = NONE;
            rtp->right->skew = RIGHT;
            break;

         default:
            rtp->skew = NONE;
            rtp->right->skew = NONE;
         }
         rtp->right->left->skew = NONE;
         avlrotright(&(rtp->right));
         avlrotleft(n);
      }
      return BT_OK;

   default:
      rtp->skew = RIGHT;
      return BT_BALANCE;
   }
}



/* FUNCTION: avlinsert()
 * 
 *  avlinsert: insert a node into the AVL tree.
 *
 *  Parameters:
 *
 *    n           Address of a pointer to a node.
 *
 *    d           Item to be inserted.
 *
 *  Return values:
 *
 *    BT_ERROR    The datum provided could not be inserted, either due 
 *                to AVLKEY collision (the tree already contains another
 *                item with which the same AVLKEY is associated), or
 *                due to insufficient memory.
 *
 *    BT_OK       None of the subtrees of the node that n points to 
 *                has grown, the AVL tree is valid.
 *
 *    BT_BALANCE  One of the subtrees of the node that n points to 
 *                has grown, the node's "skew" flag needs adjustment,
 *                and the AVL tree may have become unbalanced.
 *
 * That latter two return values are distinguished so that when avlinsert
 * recursively calls itself, the number returned tells the parent activation
 * if the AVL tree may have become unbalanced.
 *
 *
 * PARAM1: struct avl_node *n
 * PARAM2: AVLDATUM d
 *
 * RETURNS: 
 */

enum AVLRES
avlinsert(struct avl_node ** parent, struct avl_node * new)
{
   enum AVLRES tmp;
   
   if (!(*parent))
   {
      *parent = new;
      new->left = new->right = NULL;
      new->skew = NONE;
      return BT_BALANCE;
   }

   if (AVL_LESS(new, *parent))
   {
      if ((tmp = avlinsert(&(*parent)->left, new)) == BT_BALANCE) 
      {
         return avlleftgrown(parent);
      }
      return tmp;
   }
   if (AVL_GREATER(new, *parent)) 
   {
      if ((tmp = avlinsert(&(*parent)->right, new)) == BT_BALANCE) 
      {
         return avlrightgrown(parent);
      }
      return tmp;
   }
   return BT_ERROR;
}


/* FUNCTION: avlleftshrunk()
 * 
 *  avlleftshrunk: helper function for avlremove and avlfindlowest
 *
 *  Parameters:
 *
 *    n           Address of a pointer to a node. The node's left
 *                subtree has just shrunk due to item removal; its
 *                "skew" flag needs adjustment, and the local tree
 *                (the subtree of which this node is the root node) may
 *                have become unbalanced.
 *
 *   Return values:
 *
 *    BT_OK          The parent activation of the avlremove activation
 *                that called this function may assume the entire
 *                tree is valid.
 *
 *    BT_BALANCE     Do not assume the entire tree is valid.
 *
 * PARAM1: struct avl_node **n
 *
 * RETURNS: 
 */

enum AVLRES
avlleftshrunk(struct avl_node ** n)
{
   struct avl_node * rtp = *n;

   switch (rtp->skew) 
   {
   case LEFT:
      rtp->skew = NONE;
      return BT_BALANCE;

   case RIGHT:
      if (rtp->right->skew == RIGHT) 
      {
         rtp->skew = rtp->right->skew = NONE;
         avlrotleft(n);
         return BT_BALANCE;
      }
      else if (rtp->right->skew == NONE) 
      {
         rtp->skew = RIGHT;
         rtp->right->skew = LEFT;
         avlrotleft(n);
         return BT_OK;
      }
      else 
      {
         switch (rtp->right->left->skew) 
         {
         case LEFT:
            rtp->skew = NONE;
            rtp->right->skew = RIGHT;
            break;

         case RIGHT:
            rtp->skew = LEFT;
            rtp->right->skew = NONE;
            break;

         default:
            rtp->skew = NONE;
            rtp->right->skew = NONE;
         }
         rtp->right->left->skew = NONE;
         avlrotright(&(rtp->right));
         avlrotleft(n);
         return BT_BALANCE;
      }

   default:
      rtp->skew = RIGHT;
      return BT_OK;
   }
}


/* FUNCTION: avlrightshrunk()
 * 
 *  avlrightshrunk: helper function for avlremove and avlfindhighest
 *
 *  See avlleftshrunk for details.
 *
 * PARAM1: struct avl_node **n
 *
 * RETURNS: 
 */

enum AVLRES
avlrightshrunk(struct avl_node ** n)
{
   struct avl_node * rtp = *n;

   switch (rtp->skew) 
   {
   case RIGHT:
      rtp->skew = NONE;
      return BT_BALANCE;

   case LEFT:
      if (rtp->left->skew == LEFT) 
      {
         rtp->skew = rtp->left->skew = NONE;
         avlrotright(n);
         return BT_BALANCE;
      }
      else if (rtp->left->skew == NONE) 
      {
         rtp->skew = LEFT;
         rtp->left->skew = RIGHT;
         avlrotright(n);
         return BT_OK;
      }
      else 
      {
         switch (rtp->left->right->skew) 
         {
         case LEFT:
            rtp->skew = RIGHT;
            rtp->left->skew = NONE;
            break;

         case RIGHT:
            rtp->skew = NONE;
            rtp->left->skew = LEFT; 
            break;

         default:
            rtp->skew = NONE;
            rtp->left->skew = NONE;
         }
         rtp->left->right->skew = NONE;
         avlrotleft(&(rtp->left));
         avlrotright(n);
         return BT_BALANCE;
      }

   default:
      rtp->skew = LEFT;
      return BT_OK;
   }
}


/* FUNCTION: avlfindhighest()
 * 
 *  avlfindhighest: replace a node with a subtree's highest-ranking item.
 *
 *  Parameters:
 *
 *    target      Pointer to node to be replaced.
 *
 *    tree        Address of pointer to subtree.
 *
 *    res         Pointer to variable used to tell the caller whether
 *                further checks are necessary; analog to the return
 *                values of avlleftgrown and avlleftshrunk (see there). 
 *
 *
 * PARAM1: struct avl_node *target
 * PARAM2: struct avl_node **n
 * PARAM3: enum AVLRES *res
 *
 * RETURNS: 
 *
 *    1           A node was found; the target node has been replaced.
 *
 *    0           The target node could not be replaced because
 *                the subtree provided was empty.
 *
 */

int
avlfindhighest(struct avl_node * target, struct avl_node ** tree, enum AVLRES * res)
{
   struct avl_node * tmp;

   *res = BT_BALANCE;
   if (!(*tree)) 
   {
      return 0;
   }
   if ((*tree)->right) 
   {
      if (!avlfindhighest(target, &(*tree)->right, res)) 
      {
         return 0;
      }
      if (*res == BT_BALANCE) 
      {
         *res = avlrightshrunk(tree);
      }
      return 1;
   }

   /* Fall to here if tree is the highest item in the sub tree.
    * At this point the original avl code just copied the data key
    * from *tree to target. We need to copy many data items and the
    * easiest way seems to be to save target's btree info into *tree
    * and then copy all of *tree into target.
    */

   tmp = (*tree)->left;            /* save new *tree value */
   (*tree)->left = target->left;
   (*tree)->right = target->right;
   (*tree)->skew = target->skew;
   MEMCPY(target, *tree, sizeof(struct RtMib));

   (*tree)->right = 0;
   (*tree)->left = 0;
   ((RTMIB)*tree)->ipRouteNextHop = 0; /* mark empty */
   *tree = tmp;
   return 1;
}


/* FUNCTION: avlfindlowest()
 * 
 *  avlfindlowest: replace node with a subtree's lowest-ranking item.
 *
 *  See avlfindhighest for the details.
 *
 * PARAM1: struct avl_node *target
 * PARAM2: struct avl_node **n
 * PARAM3: enum AVLRES *res
 *
 * RETURNS: 
 */

int
avlfindlowest(struct avl_node * target, struct avl_node **tree, enum AVLRES * res)
{
   struct avl_node * tmp;

   *res = BT_BALANCE;
   if (!(*tree))
   {
      return 0;
   }
   if ((*tree)->left)
   {
      if (!avlfindlowest(target, &(*tree)->left, res)) 
      {
         return 0;
      }
      if (*res == BT_BALANCE) 
      {
         *res =  avlleftshrunk(tree);
      }
      return 1;
   }
   tmp = (*tree)->right;            /* save new *tree value */
   (*tree)->left = target->left;
   (*tree)->right = target->right;
   (*tree)->skew = target->skew;
   MEMCPY(target, *tree, sizeof(struct RtMib));

   (*tree)->right = 0;
   (*tree)->left = 0;
   ((RTMIB)*tree)->ipRouteNextHop = 0; /* mark empty */
   *tree = tmp;
   return 1;
}



/* FUNCTION: avlremove()
 * 
 *  avlremove: remove an item from the tree.
 *
 *  Parameters:
 *
 *    n           trunk of AVL tree.
 *
 *    key         AVLKEY of item to be removed.
 *
 *
 * PARAM1: struct avl_node **n
 * PARAM2: int key
 *
 * RETURNS: 
 *
 *   BT_OK        None of the subtrees of the node that n points to
 *                has shrunk, the AVL tree is valid.
 *
 *   BT_BALANCE   One of the subtrees of the node that n points to
 *                has shrunk, the node's "skew" flag needs adjustment,
 *                and the AVL tree may have become unbalanced.
 *
 *   BT_ERROR     The tree does not contain an item yielding the
 *                AVLKEY value provided by the caller.
 *
 * If return is not BT_ERROR then the item has been removed. The exact
 * value of nonzero yields is of no concern to user code; when
 * avlremove recursively calls itself, the number returned tells
 * the parent activation if the AVL tree may have become unbalanced.
 */

enum AVLRES
avlremove(struct avl_node **n, void * key)
{
   enum AVLRES tmp;

#ifdef NPDEBUG
   if (!n || !(*n)) 
   {
      dtrap();
      return BT_ERROR;
   }
#endif /* NPDEBUG */

    /* end recur at (*n) == 0 */
    if (*n == 0)
   	  return BT_OK;

   /* recurse if IP to delete is on left side of tree (lower) */
   if (AVL_KEY_LESS(key, (*n)))
   {
      if ((tmp = avlremove(&(*n)->left, key)) == BT_BALANCE) 
      {
         return avlleftshrunk(n);
      }
      return tmp;
   }

   /* recurse if IP to delete is on right side of tree (higher) */
   if (AVL_KEY_GREATER(key, (*n)))
   {
      if ((tmp = avlremove(&(*n)->right, key)) == BT_BALANCE) 
      {
         return avlrightshrunk(n);
      }
      return tmp;
   }

   /* get to here if *n contains IP to delete */

   /* find node with which to replace n. If n has a left (lower)
    * branch, move it up to  replace n; else use right.
    */
   if ((*n)->left)
   {
      if (avlfindhighest(*n, &(*n)->left, &tmp)) 
      {
         if (tmp == BT_BALANCE) 
         {
            tmp = avlleftshrunk(n);
         }
         return tmp;
      }
   }
   if ((*n)->right)
   {
      if (avlfindlowest(*n, &(*n)->right, &tmp)) 
      {
         if (tmp == BT_BALANCE) 
         {
            tmp = avlrightshrunk(n);
         }
         return tmp;
      }
   }

   (*n)->right = 0;
   (*n)->left = 0;
   ((RTMIB)*n)->ipRouteNextHop = 0; /* mark empty */
   *n = NULL;
   return BT_BALANCE;
}


/* FUNCTION: avlv4_access()
 * FUNCTION: avlv6_access()
 * 
 *  Retrieve the route entry for IP address. The v4 version takes a
 * v4 address; the v6 version takes a pointer to a v6 address.
 *
 *
 * PARAM1: struct avl_node * n - Pointer to the root node.
 * PARAM2: ip_addr key      - IP address to be accessed.
 *
 * RETURNS: pointer to route table entry containing passed IP address if
 *          found, else NULL if not found.
 *
 */

struct avl_node *
avlv4_access(struct avl_node * n, ip_addr key)
{
   if (!n)
   {
      return NULL;
   }
   if (key < htonl(((RTMIB)n)->ipRouteDest) )
   {
      return avlv4_access(n->left, key);
   }
   if (key > htonl(((RTMIB)n)->ipRouteDest) )
   {
      return avlv4_access(n->right, key);
   }
   return (n);
}

#ifdef IP_V6
struct avl_node *
avlv6_access(struct avl_node * n, ip6_addr * key)
{
   int result;

   if (!n)
   {
      return NULL;
   }

   result = MEMCMP(key, &((struct ipv6Route_mib *)n)->ipv6RouteDest, 16);
   if (result < 0)
   {
      return avlv6_access(n->left, key);
   }
   if (result > 0)
   {
      return avlv6_access(n->right, key);
   }
   return (n);
}
#endif   /* IP_V6 */


/* FUNCTION: avldepthfirst()
 * 
 *  avldepthfirst: depth-first tree traversal.
 *
 *  Parameters:
 *
 *    n          Pointer to the root node.
 *
 *    f          Worker function to be called for every node.
 *
 *    param      Additional parameter to be passed to the
 *               worker function
 *
 *    depth      Recursion depth indicator. Allows the worker function
 *               to determine how many levels the node being processed
 *               is below the root node. Can be used, for example,
 *               for selecting the proper indentation width when
 *               avldepthfirst ist used to print a tree dump to
 *               the screen.
 *
 *               Most of the time, you will want to call avldepthfirst
 *               with a "depth" value of zero.
 *
 * PARAM1: struct avl_node *n
 * PARAM2: AVLWORKER *f
 * PARAM3: int param
 * PARAM4: int depth
 *
 * RETURNS: 
 */

void
avldepthfirst(
   struct avl_node * n,
   void(*func)(struct avl_node * n, long param, int depth),
   long param,
   int depth)
{
   if(!n)
      return;

   avldepthfirst(n->left, func, param, depth + 1);
   (*func)(n, param, depth);
   avldepthfirst(n->right, func, param, depth +1);
}

#ifdef NOTDEF
/* FUNCTION: avlbreadthfirst()
 * 
 *  avlbreadthfirst: breadth-first tree traversal.
 * 
 *  See avldepthfirst for details.
 *
 * PARAM1: struct avl_node *n
 * PARAM2: AVLWORKER *f
 * PARAM3: int param
 *
 * RETURNS: 
 */

void
avlbreadthfirst(struct avl_node * n, AVLWORKER * f, int param)
{
    struct queue   q;

   if (!n) return;
      qinit(&q);
   qappend(&q, n);
   while (qremove(&q, &n)) 
   {
      (*f)(n, param, 0);
      if (n->left) qappend(&q, n->left);
         if (n->right) qappend(&q, n->right);
   }
}
#endif   /* NOTDEF */

#ifdef IP_V6

/* The next four routines are macros on IPv4 only builds. On
 * builds which include IPv6, they are implemented by the following
 * code, which contains #ifdefs for IPv4 (dual mode) support.
 */

int
AVL_LESS(struct avl_node * a, struct avl_node * b)
{
   int result;

#ifdef IP_V4
   if(a->treetype == IPV4_ROUTE)
      return((htonl(((RTMIB)a)->ipRouteDest) < htonl(((RTMIB)b)->ipRouteDest)));
#endif   /* IP_V4 */

   /* fall to here if the route type is IPv6 */
   result = MEMCMP(&((ip6_route *)a)->ipv6RouteDest, &((ip6_route *)b)->ipv6RouteDest, 16);

   if(result < 0)
      return TRUE;
   else
      return FALSE;
}

int
AVL_GREATER(struct avl_node * a, struct avl_node * b)
{
   int result;

#ifdef IP_V4
   if(a->treetype == IPV4_ROUTE)
      return((htonl(((RTMIB)a)->ipRouteDest) > htonl(((RTMIB)b)->ipRouteDest)));
#endif   /* IP_V4 */

   /* fall to here if the route type is IPv6 */
   result = MEMCMP(&((ip6_route *)a)->ipv6RouteDest, &((ip6_route *)b)->ipv6RouteDest, 16);

   if(result > 0)
      return TRUE;
   else
      return FALSE;
}

int
AVL_KEY_LESS(void * key, struct avl_node * b)
{
   int result;

#ifdef IP_V4
   if(b->treetype == IPV4_ROUTE)
      return(((*(ip_addr*)key) < htonl(((RTMIB)b)->ipRouteDest)));
#endif   /* IP_V4 */

   /* fall to here if the route type is IPv6 */
   result = MEMCMP(key, &((ip6_route *)b)->ipv6RouteDest, 16);

   if(result < 0)
      return TRUE;
   else
      return FALSE;
}

int
AVL_KEY_GREATER(void * key, struct avl_node * b)
{
   int result;

#ifdef IP_V4
   if(b->treetype == IPV4_ROUTE)
      return(((*(ip_addr*)key) > htonl(((RTMIB)b)->ipRouteDest)));
#endif   /* IP_V4 */

   /* fall to here if the route type is IPv6 */
   result = MEMCMP(key, &((ip6_route *)b)->ipv6RouteDest, 16);

   if(result > 0)
      return TRUE;
   else
      return FALSE;
}
#endif   /* IP_V6 */


#endif /* BTREE_ROUTES */

