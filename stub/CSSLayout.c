#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <assert.h>
#include <stdio.h>
#include "CSSLayout.h"

#define assert__(x) for ( ; !(x) ; assert(x) )

// Look for a ocaml method with the same name as function name, and memorize it
#define camlMethod(x) \
    camlMethodWithName(x, __func__)

#define camlMethodWithName(x, name) \
    static value * x = NULL; \
    if (x == NULL) x = caml_named_value(name); \
    assert__(x) { \
        printf("FATAL: function %s not implemented in OCaml, check bindings.re\n", name);  \
    };

char* itoa(uintnat val, int base){

    static char buf[65] = {0};

    int i = 64;

    for(; i ; --i, val /= base)

        buf[i] = "0123456789abcdef"[val % base];

    return &buf[i+1];

}

static void printBinary(uintnat i) {
    char *buffer = itoa (i,2);
    printf ("binary: %s\n",buffer);
}

// Ocaml's macro system only supports up to 3 arguments, so we have to write one by ourselves here.
// TODO: There definitely is a better way to macro this.
value caml_callback4 (value closure, value arg1, value arg2,
                      value arg3, value arg4) {
    value arg[4];
    arg[0] = arg1;
    arg[1] = arg2;
    arg[2] = arg3;
    arg[3] = arg4;
    value res = caml_callbackN_exn(closure, 4, arg);
    if (Is_exception_result(res)) caml_raise(Extract_exception(res));
    return res;
}

static int32_t gNodeInstanceCount = 0;

inline bool CSSValueIsUndefined(const float v) {
    return isnan(v);
}

#define bridgeEnumToCamlVal(type)               \
    inline value type##ToCamlVal(const type v) {\
        return Val_int(v);                      \
    }                                           \
    inline type CamlValTo##type(value v) {      \
        return Int_val(v);                      \
    }

bridgeEnumToCamlVal(CSSAlign)
bridgeEnumToCamlVal(CSSJustify)
bridgeEnumToCamlVal(CSSDirection)
bridgeEnumToCamlVal(CSSEdge)
bridgeEnumToCamlVal(CSSFlexDirection)
bridgeEnumToCamlVal(CSSMeasureMode)
bridgeEnumToCamlVal(CSSWrapType)
bridgeEnumToCamlVal(CSSPositionType)
bridgeEnumToCamlVal(CSSOverflow)


static value Min_int;
__attribute__ ((__constructor__))
void initMinInt(void) {
    camlMethodWithName(minInt, "minInt");
    Min_int = caml_callback(*minInt, Val_unit);
}

inline value floatToCamlVal(const float v) {
    if (CSSValueIsUndefined(v)) {
        return Min_int;
    }
    return Val_int(v * 100);
}

static float CamlValTofloat(value v) {
    if (v == Min_int) {
        return CSSUndefined;
    }
    return Int_val(v) / 100;
}

#define scale_factor 100;

CSSNodeRef CSSNodeNew(void) {
    CAMLparam0();
    CAMLlocal1(v);
    value *valp;
    camlMethod(closure);
    valp = (value *) malloc(sizeof *valp);
    v = caml_callback(*closure, caml_copy_nativeint((intnat)valp));
    *valp = v;
    gNodeInstanceCount++;
    // Register the value with global heap
    caml_register_global_root(valp);
    CAMLreturnT(value *, valp);
}

void CSSNodeReset(const CSSNodeRef node){
    // - Create a new, dummy node and assign it to the pointer
    // - Old value should automatically be GCed
    // - No need to call caml_register_global_root again as the pointer remain the same
    camlMethodWithName(closure, "CSSNodeNew");
    *node = caml_callback(*closure, Val_unit);
}

void CSSNodeSetMeasureFunc(const CSSNodeRef node, CSSMeasureFunc measureFunc) {
    CAMLparam0();
    CAMLlocal1(v);
    camlMethod(closure);
    v = caml_copy_nativeint((intnat)measureFunc);
    caml_callback2(*closure, *node, v);
    CAMLreturn0;
}

CSSMeasureFunc CSSNodeGetMeasureFunc(const CSSNodeRef node) {
    camlMethod(closure);
    return (CSSMeasureFunc)Nativeint_val(caml_callback(*closure, *node));
}

void CSSNodeSetHasNewLayout(const CSSNodeRef node, bool hasNewLayout) {
    camlMethod(closure);
    caml_callback(*closure, Val_int(hasNewLayout));
}

void CSSNodeSetContext(const CSSNodeRef node, void *context) {
    CAMLparam0();
    CAMLlocal1(v);
    camlMethod(closure);
    v = caml_copy_nativeint((intnat)context);
    caml_callback2(*closure, *node, v);
    CAMLreturn0;
}

void *CSSNodeGetContext(const CSSNodeRef node) {
    camlMethod(closure);
    return (void *)Nativeint_val(caml_callback(*closure, *node));
}

bool CSSNodeGetHasNewLayout(const CSSNodeRef node) {
    camlMethod(closure);
    return Int_val(caml_callback(*closure, *node));
}

static CSSNodeRef CSSNodeGetSelfRef(value node) {
    camlMethod(closure);
    return (CSSNodeRef)Nativeint_val(caml_callback(*closure, node));
}

int32_t CSSNodeGetInstanceCount(void) {
    return gNodeInstanceCount;
}

void CSSNodeInit(const CSSNodeRef node) {
    // all objects from ocaml are already inited. This is an noop
    return;
}

void CSSNodeFree(const CSSNodeRef node) {
    gNodeInstanceCount--;
    // deregister the value with global heap
    caml_remove_global_root(node);
    free(node);
}

// On the contract, the ownership of a node always belong to the creator.
// This function, however assumes the ownership of a node belong to the tree.
// We have this function mostly for convenience purpose in unit tests.
void CSSNodeFreeRecursive(const CSSNodeRef node) {
    // deregister the value with global heap, children of this object still need to be freed
    // by its owner.
    CSSNodeFree(node);
}

void CSSNodeInsertChild(const CSSNodeRef node,
                        const CSSNodeRef child,
                        const uint32_t index) {
    // We have no local ocaml allocation here, so no need for CAMLparam/CAMLreturn/etc
    camlMethod(closure);
    caml_callback3(*closure, *node, *child, Val_int(index));
    return;
}

void CSSNodeRemoveChild(const CSSNodeRef node,
                        const CSSNodeRef child) {
    // We have no local ocaml allocation here, so no need for CAMLparam/CAMLreturn/etc
    CAMLparam0();
    CAMLlocal1(v);
    v = caml_copy_nativeint((intnat)child);
    camlMethod(closure);
    caml_callback2(*closure, *node, v);
    CAMLreturn0;
}

uint32_t CSSNodeChildCount(const CSSNodeRef node) {
    camlMethod(closure);
    value v = caml_callback(*closure, *node);
    return (uint32_t)Int_val(v);
}

void CSSNodeCalculateLayout(const CSSNodeRef node,
                            const float availableWidth,
                            const float availableHeight,
                            const CSSDirection parentDirection) {
    camlMethod(closure);
    caml_callback4(*closure, *node, floatToCamlVal(availableWidth),
                  floatToCamlVal(availableWidth),
                  CSSDirectionToCamlVal(parentDirection));
    return;
}

CSSNodeRef CSSNodeGetChild(const CSSNodeRef node,
                           const uint32_t index) {
    // We have no local ocaml allocation here, so no need for CAMLparam/CAMLreturn/etc
    camlMethod(closure);
    return (CSSNodeRef)Nativeint_val(caml_callback2(*closure,
                                                    *node, Val_int(index)));
}

void CSSNodeMarkDirty(const CSSNodeRef node) {
    // TODO: implement this
    return;
}

bool CSSNodeIsDirty(const CSSNodeRef node) {
    camlMethod(closure);
    return Bool_val(caml_callback(*closure, *node));
}

void CSSNodePrint(const CSSNodeRef node,
                  const CSSPrintOptions options) {
    // TODO: implement this
}

bool CSSNodeCanUseCachedMeasurement(const CSSMeasureMode widthMode,
                                    const float width,
                                    const CSSMeasureMode heightMode,
                                    const float height,
                                    const CSSMeasureMode lastWidthMode,
                                    const float lastWidth,
                                    const CSSMeasureMode lastHeightMode,
                                    const float lastHeight,
                                    const float lastComputedWidth,
                                    const float lastComputedHeight,
                                    const float marginRow,
                                    const float marginColumn) {
    // TODO: implement this
    return false;
}

/* Padding */
void CSSNodeStyleSetPadding(const CSSNodeRef node, CSSEdge edge, float v) {
    camlMethod(closure);
    caml_callback3(*closure, *node, CSSEdgeToCamlVal(edge), floatToCamlVal(v));
}

void CSSNodeStyleSetPosition(const CSSNodeRef node, CSSEdge edge, float v) {
    camlMethod(closure);
    caml_callback3(*closure, *node, CSSEdgeToCamlVal(edge), floatToCamlVal(v));
}

void CSSNodeStyleSetMargin(const CSSNodeRef node, CSSEdge edge, float v) {
    camlMethod(closure);
    caml_callback3(*closure, *node, CSSEdgeToCamlVal(edge), floatToCamlVal(v));
}

void CSSNodeStyleSetBorder(const CSSNodeRef node, CSSEdge edge, float v) {
    camlMethod(closure);
    caml_callback3(*closure, *node, CSSEdgeToCamlVal(edge), floatToCamlVal(v));
}


float CSSNodeStyleGetPadding(const CSSNodeRef node, CSSEdge edge) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback2(*closure, *node, CSSEdgeToCamlVal(edge)));
}

float CSSNodeStyleGetMargin(const CSSNodeRef node, CSSEdge edge) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback2(*closure, *node, CSSEdgeToCamlVal(edge)));
}

float CSSNodeStyleGetPosition(const CSSNodeRef node, CSSEdge edge) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback2(*closure, *node, CSSEdgeToCamlVal(edge)));
}

float CSSNodeStyleGetBorder(const CSSNodeRef node, CSSEdge edge) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback2(*closure, *node, CSSEdgeToCamlVal(edge)));
}

/* Style */
#define defineNodeStyle(type, name)                                     \
    void CSSNodeStyleSet##name(const CSSNodeRef node, const type v) {   \
        camlMethod(closure);                                            \
        caml_callback2(*closure, *node, type##ToCamlVal(v));            \
        return;                                                         \
    }                                                                   \
    type CSSNodeStyleGet##name(const CSSNodeRef node) {                 \
        camlMethod(closure);                                            \
        value v = caml_callback(*closure, *node);                       \
        return CamlValTo##type(caml_callback(*closure, *node));         \
    }                                                                   \

/* Style */

defineNodeStyle(CSSJustify, JustifyContent)

defineNodeStyle(CSSAlign, AlignItems)

defineNodeStyle(CSSAlign, AlignContent)

defineNodeStyle(CSSAlign, AlignSelf)

defineNodeStyle(CSSDirection, Direction)

defineNodeStyle(CSSPositionType, PositionType)

defineNodeStyle(CSSWrapType, FlexWrap)

defineNodeStyle(CSSFlexDirection, FlexDirection)

defineNodeStyle(CSSOverflow, Overflow)

defineNodeStyle(float, Width);

defineNodeStyle(float, MaxWidth);

defineNodeStyle(float, MaxHeight);

defineNodeStyle(float, MinWidth);

defineNodeStyle(float, MinHeight);

defineNodeStyle(float, Height);

defineNodeStyle(float, FlexGrow);

defineNodeStyle(float, FlexShrink);

defineNodeStyle(float, FlexBasis);

defineNodeStyle(float, Flex);


/* Layout */
float CSSNodeLayoutGetWidth(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

float CSSNodeLayoutGetHeight(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

float CSSNodeLayoutGetTop(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

float CSSNodeLayoutGetBottom(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

float CSSNodeLayoutGetLeft(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

float CSSNodeLayoutGetRight(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValTofloat(caml_callback(*closure, *node));
}

CSSDirection CSSNodeLayoutGetDirection(const CSSNodeRef node) {
    camlMethod(closure);
    return CamlValToCSSDirection(caml_callback(*closure, *node));
}

// This is a special case for OCaml functions that have more than 5 parameters, in such cases you have to provide 2 C functions

// This defines a stub api for ocaml to call back, it then passes control to the C function pointer
CAMLprim value cssMeasureFFI(value ptr, value node, value w, value wm, value h, value hm) {
    CSSMeasureFunc f = (CSSMeasureFunc)Nativeint_val(ptr);
    CSSSize s = f((CSSNodeRef)Nativeint_val(node),
                  CamlValTofloat(w),
                  CamlValToCSSMeasureMode(wm),
                  CamlValTofloat(h),
                  CamlValToCSSMeasureMode(hm));
    camlMethodWithName(getMeasurement, "GetMeasurement");
    return caml_callback2(*getMeasurement, floatToCamlVal(w), floatToCamlVal(h));
}

CAMLprim value cssMeasureFFI_bytecode(value * argv, int argn) {
    return cssMeasureFFI(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}
