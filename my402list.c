/*
 * Author:     Steve Regala (sregala@usc.edu)
 *
 */

#include "cs402.h"
#include "my402list.h"
#include "stdlib.h"
#include "stdio.h"

/*
Returns the number of elements in the list.
PRE: ourList must be a valid list
*/
int  My402ListLength(My402List* ourList){	
	return ourList->num_members;
}

/*
Returns TRUE if the list is empty. Returns FALSE otherwise.
PRE: ourList must be a valid list
*/
int  My402ListEmpty(My402List* ourList){
	return 	ourList->num_members <= 0;
}

/*
If list is empty, just add obj to the list. Otherwise, add obj after Last(). 
This function returns TRUE if the operation is performed successfully and returns FALSE otherwise.
PRE: ourList must be a valid list
POST: num_members must increase in size
*/
int  My402ListAppend(My402List* ourList, void *obj){
	My402ListElem *elem = (My402ListElem*)malloc(sizeof(My402ListElem));
	elem->obj = obj;
	
	/* 
	a.b is used if b is a member of the object, a will always be an actual object (or a
	reference to an object) of a class
	a->b is if a is a pointer to an object, then a->b is accessing the property b of the 
	object that a points to
	*/
	if (My402ListEmpty(ourList)) {
		elem->prev = &(ourList->anchor);
		elem->next = &(ourList->anchor);
		(ourList->anchor).prev = elem;
		(ourList->anchor).next = elem;
		ourList->num_members++;
		return TRUE;
	}

	else {
		My402ListElem* lastElem = My402ListLast(ourList);
		lastElem->next = elem;
		elem->prev = lastElem;
		elem->next = &(ourList->anchor);
		(ourList->anchor).prev = elem;
		ourList->num_members++;
		return TRUE;
	}
	return FALSE;
	
}


/*
If list is empty, just add obj to the list. Otherwise, add obj before First(). 
This function returns TRUE if the operation is performed successfully and returns FALSE otherwise.
PRE: ourList must be a valid list
POST: num_members must increase in size
*/
int  My402ListPrepend(My402List* ourList, void* obj){
	My402ListElem *elem = (My402ListElem*)malloc(sizeof(My402ListElem));
	elem->obj = obj;

	if (My402ListEmpty(ourList)) {
		elem->prev = &(ourList->anchor);
		elem->next = &(ourList->anchor);
		(ourList->anchor).prev = elem;
		(ourList->anchor).next = elem;
		ourList->num_members++;
		return TRUE;	
	}
	
	else {
		My402ListElem* firstElem = My402ListFirst(ourList);
		firstElem->prev = elem;
		elem->next = firstElem;
		elem->prev = &(ourList->anchor);
		(ourList->anchor).next = elem;
		ourList->num_members++;
		return TRUE;
	}	
	return FALSE;

}

/*
Unlink and free() elem from the list. 
PRE: ourList must be a valid list and elem cannot be null
POST: num_members must decrease in size
*/
void My402ListUnlink(My402List* ourList, My402ListElem* elem){
	// need to repoint elem's next and prev accordingly
	My402ListElem* prevTemp = elem->prev;
	My402ListElem* nextTemp = elem->next;
	prevTemp->next = nextTemp;
	nextTemp->prev = prevTemp;
	free(elem);
	ourList->num_members--;
}

/*
Unlink and free() all elements from the list and make the list empty.
PRE: ourList must be a valid list
POST: num_members must be 0
*/
void My402ListUnlinkAll(My402List* ourList){
	My402ListElem *currElem = NULL;
	
	for (currElem=My402ListFirst(ourList); currElem != NULL; currElem=My402ListNext(ourList, currElem)) {		
		My402ListUnlink(ourList, currElem);
	}
}

/*
Insert obj between elem and elem->next. If elem is NULL, then this is the same as Append(). 
This function returns TRUE if the operation is performed successfully and returns FALSE otherwise.
PRE: ourList must be a valid list
POST: num_members must increase in size
*/
int  My402ListInsertAfter(My402List* ourList, void* obj, My402ListElem* elem){

	if (elem==NULL) {
		return My402ListAppend(ourList, obj);
	}

	else {
		My402ListElem *insertElem = (My402ListElem*)malloc(sizeof(My402ListElem));
		insertElem->obj = obj;

		insertElem->next = elem->next;
		insertElem->prev = elem;
		elem->next->prev = insertElem;		
		elem->next = insertElem;
		ourList->num_members++;
		return TRUE;
	}
	return FALSE;	

}

/*
Insert obj between elem and elem->prev. If elem is NULL, then this is the same as Prepend(). 
This function returns TRUE if the operation is performed successfully and returns FALSE otherwise.
PRE: ourList must be a valid list
POST: num_members must increase in size
*/
int  My402ListInsertBefore(My402List* ourList, void* obj, My402ListElem* elem){

	if (elem==NULL) {
		return My402ListPrepend(ourList, obj); 
	}
	
	else {
		My402ListElem *insertElem = (My402ListElem*)malloc(sizeof(My402ListElem));
		insertElem->obj = obj;

		insertElem->next = elem;
		insertElem->prev = elem->prev;
		elem->prev->next = insertElem;
		elem->prev = insertElem;
		ourList->num_members++;
		return TRUE;
	}
	return FALSE;	

}

/*
Returns the first list element or NULL if the list is empty.
PRE: ourList must be a valid list
*/
My402ListElem *My402ListFirst(My402List* ourList){
	if (My402ListEmpty(ourList)) {
		return NULL;		
	}
	return (ourList->anchor).next;
}

/*
Returns the last list element or NULL if the list is empty.
PRE: ourList must be a valid list
*/
My402ListElem *My402ListLast(My402List* ourList){
	if (My402ListEmpty(ourList)) {
		return NULL;		
	}
	return (ourList->anchor).prev;
}

/*
Returns elem->next or NULL if elem is the last item on the list.
PRE: ourList must be a valid list, elem must be on the list
*/
My402ListElem *My402ListNext(My402List* ourList, My402ListElem* elem){
	if (My402ListEmpty(ourList) || elem==My402ListLast(ourList)) {
		return NULL;	
	}
	return elem->next;
}

/*
Returns elem->prev or NULL if elem is the first item on the list.
PRE: ourList must be a valid list, elem must be on the list
*/
My402ListElem *My402ListPrev(My402List* ourList, My402ListElem* elem){
	if (My402ListEmpty(ourList) || elem==My402ListFirst(ourList)) {
		return NULL;	
	}
	return elem->prev;
}

/*
Returns the list element elem such that elem->obj == obj. Returns NULL if no such element can be found.
PRE: ourList must be a valid list
*/
My402ListElem *My402ListFind(My402List* ourList, void* obj){
	My402ListElem *currElem = NULL;

	for (currElem=My402ListFirst(ourList); currElem != NULL; currElem=My402ListNext(ourList, currElem)) {
		if (currElem->obj == obj) {
			return currElem;
		}
	}
	return NULL;
}

/*
Initialize the list into an empty list. 
Returns TRUE if all is well and returns FALSE if there is an error initializing the list.
PRE: ourList must be a valid list
POST: num_members must be 0
*/
int My402ListInit(My402List* ourList){
	if (ourList == NULL) {
		return FALSE;	
	}
	My402ListUnlinkAll(ourList);

	ourList->num_members=0;
	(ourList->anchor).obj=NULL;
	(ourList->anchor).next=NULL;
	(ourList->anchor).prev=NULL;

	return TRUE;
}
