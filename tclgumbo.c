#include "tcl.h"
#include "gumbo.h"

typedef struct TclGumboHandle {
	struct TclGumboHandle *root;
	GumboOutput *groot;
	GumboNode *gn;
} TclGumboHandle;

TclGumboHandle *getHandle(Tcl_HashTable *t, char *handle) {
	Tcl_HashEntry *e=Tcl_FindHashEntry(t,handle);
	if (e!=NULL) {
		return (TclGumboHandle *)Tcl_GetHashValue(e);
	} else {
		return NULL;
	}
}

char *putHandle(Tcl_HashTable *t, TclGumboHandle *handle) {
	Tcl_HashEntry *e;
	int new;
	char *key=ckalloc(24);
	sprintf(key,"gumbo%p",handle);
	e=Tcl_CreateHashEntry(t,key,&new);
	if (e!=NULL) {
		Tcl_SetHashValue(e,handle);
		return key;
	} else {
		return NULL;
	}
}

void deleteHandle(Tcl_HashTable *t, char *handle) {
	Tcl_HashEntry *e=Tcl_FindHashEntry(t,handle);
	if (e!=NULL) {
		TclGumboHandle *h=(TclGumboHandle *)Tcl_GetHashValue(e);
		// ckfree(h->gn);
		Tcl_DeleteHashEntry(e);
	}
}


void
TclGumbo_Fini(void *cd) {
	Tcl_DeleteHashTable((Tcl_HashTable *)cd);
	ckfree(cd);
}

/* 
 * gumbo parse <text> -> handle (gumbonode)
 * gumbo children <handle> -> child list
 * gumbo type <handle> -> element|text
 * gumbo tag <element handle> -> normalized tag
 * gumbo attributes <element handle> -> attribute name list
 * gumbo getattribute <element handle> <attr> -> attr value
 * gumbo text <text handle> -> text
 * gumbo next <handle> -> next handle
 * gumbo destroy <handle> 
 */
static int
Tclgumbo_Cmd(void *cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	int opt;
	Tcl_HashTable *handles=cd;
	TclGumboHandle *gh;
	GumboOutput *go;
	char *text, *handle;
	Tcl_Obj *oh;

	static const char *cmds[]={
		"parse", "destroy", "type",
		"tag", "attributes", "getattribute", "children",
		"text",
		"next"
	};
	enum CmdIx {
		CParseIdx, CDestroyIdx, CTypeIdx,
		CTagIdx, CAttributesIdx, CGetattributeIdx, CChildrenIdx, 
		CTextIdx,
		CNextIdx
	};
	if (objc < 3) {
		Tcl_WrongNumArgs(interp,1,objv,"command arg");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp, objv[1], cmds, "command", 0, &opt) != TCL_OK) {
		return TCL_ERROR;
	}

	if (opt == CGetattributeIdx) {
		if (objc < 4) {
			Tcl_WrongNumArgs(interp,2,objv,"getattribute handle attribute");
			return TCL_ERROR;
		}
	}
	if (opt != CParseIdx) {
		handle=Tcl_GetString(objv[2]);
		gh=getHandle(handles,handle);
	}

	switch (opt) {
		case CParseIdx: {
			int l;
			text=Tcl_GetStringFromObj(objv[2],&l);
			gh=ckalloc(sizeof(TclGumboHandle));
			go=gumbo_parse_with_options(&kGumboDefaultOptions,text,l);
			gh->gn=go->root;
			gh->root=gh;
			gh->groot=go;
			oh=Tcl_NewStringObj(putHandle(handles,gh),-1);
			Tcl_SetObjResult(interp,oh);
			break;
		}

		case CDestroyIdx:
			gumbo_destroy_output(&kGumboDefaultOptions, gh->groot);
			deleteHandle(handles,handle);
			break;
			
		case CChildrenIdx: {
			GumboVector ch;
			TclGumboHandle *cgh;
			Tcl_Obj *l,*e;
			int c;

			l=Tcl_NewListObj(0,NULL);
			ch=gh->gn->v.element.children;
			for(c=0; c < ch.length; c++) {
				cgh=ckalloc(sizeof(TclGumboHandle));
				cgh->gn=(GumboNode *)(ch.data)[c];
				cgh->root=gh->root;
				cgh->groot=gh->groot;
				e=Tcl_NewStringObj(putHandle(handles,cgh),-1);
				// Tcl_IncrRefCount(e);
				Tcl_ListObjAppendElement(interp,l,e);
			}
			Tcl_SetObjResult(interp,l);
			break;
		}

		case CTypeIdx:
			if (gh->gn->type == GUMBO_NODE_ELEMENT) {
				Tcl_SetResult(interp,"element",TCL_STATIC);
			} else {
				Tcl_SetResult(interp,"text",TCL_STATIC);
			}
			break;

		case CTagIdx:
			Tcl_SetResult(interp,(char *)gumbo_normalized_tagname(gh->gn->v.element.tag),TCL_STATIC);
			break;
			
		case CAttributesIdx: {
			GumboVector av;
			Tcl_Obj *l,*e;
			int c;

			l=Tcl_NewListObj(0,NULL);
			av=gh->gn->v.element.attributes;
			for(c=0; c < av.length; c++) {
				e=Tcl_NewStringObj(((GumboAttribute *)(av.data)[c])->name, -1);
				// Tcl_IncrRefCount(e);
				Tcl_ListObjAppendElement(interp,l,e);
			}
			Tcl_SetObjResult(interp,l);
			break;
		}

		case CGetattributeIdx: {
			GumboAttribute *ga;
			char *att=Tcl_GetString(objv[3]);
			ga=gumbo_get_attribute(&(gh->gn->v.element.attributes), att);
			if (ga != NULL) {
				Tcl_SetResult(interp,(char *)ga->value,TCL_STATIC);
			} else {
				Tcl_SetResult(interp,"",TCL_STATIC);
			}
			break;
		}

		case CTextIdx:
			Tcl_SetResult(interp,(char *)gh->gn->v.text.text,TCL_STATIC);
			break;

		case CNextIdx: {
			TclGumboHandle *ngh;
			if (gh->gn->next == NULL) {
				Tcl_SetResult(interp,"",TCL_STATIC);
				break;
			}
			ngh=ckalloc(sizeof(TclGumboHandle));
			ngh->gn=(gh->gn->next);
			ngh->root=gh->root;
			ngh->groot=gh->groot;
			Tcl_SetResult(interp,putHandle(handles,ngh),TCL_STATIC);
			break;
		}

		default:
			break;
	}
	return TCL_OK;
}

int
Tclgumbo_Init(Tcl_Interp *interp) {
	Tcl_HashTable *handles;
	handles=ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(handles, TCL_STRING_KEYS);

	Tcl_CreateObjCommand(interp, "gumbo", Tclgumbo_Cmd, handles, TclGumbo_Fini);
	return TCL_OK;
}
