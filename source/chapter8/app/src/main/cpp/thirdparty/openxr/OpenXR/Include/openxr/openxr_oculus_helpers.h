// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

/************************************************************************************

Filename    :   openxr_oculus_helpers.h
Content     :   OpenXR Helper routines
Language    :   C99

*************************************************************************************/

#ifndef OPENXR_OCULUS_HELPERS_H
#define OPENXR_OCULUS_HELPERS_H 1

#include <openxr/openxr.h>
#include <xr_linear.h>

#include "math.h" // for cosf(), sinf(), tanf()

// OpenXR time values are expressed in nanseconds.
static inline double FromXrTime(const XrTime time) {
    return (time * 1e-9);
}

static inline XrTime ToXrTime(const double timeInSeconds) {
    return (timeInSeconds * 1e9);
}

static inline XrVector3f XrVector3f_Zero() {
    XrVector3f r;
    r.x = r.y = r.z = 0.0f;
    return r;
}

static inline XrVector3f XrVector3f_ScalarMultiply(const XrVector3f v, float scale) {
    XrVector3f u;
    u.x = v.x * scale;
    u.y = v.y * scale;
    u.z = v.z * scale;
    return u;
}

static inline float XrVector3f_LengthSquared(const XrVector3f v) {
    return XrVector3f_Dot(&v, &v);
}

static inline XrVector3f XrVector3f_Normalized(const XrVector3f v) {
    float rcpLen = 1.0f / XrVector3f_Length(&v);
    return XrVector3f_ScalarMultiply(v, rcpLen);
}

static inline XrQuaternionf XrQuaternionf_Identity() {
    XrQuaternionf r;
    r.x = r.y = r.z = 0.0;
    r.w = 1.0f;
    return r;
}

static inline XrQuaternionf XrQuaternionf_CreateFromVectorAngle(
    const XrVector3f axis,
    const float angle) {
    XrQuaternionf r;
    if (XrVector3f_LengthSquared(axis) == 0.0f) {
        return XrQuaternionf_Identity();
    }

    XrVector3f unitAxis = XrVector3f_Normalized(axis);
    float sinHalfAngle = sinf(angle * 0.5f);

    r.w = cosf(angle * 0.5f);
    r.x = unitAxis.x * sinHalfAngle;
    r.y = unitAxis.y * sinHalfAngle;
    r.z = unitAxis.z * sinHalfAngle;
    return r;
}

static inline XrQuaternionf XrQuaternionf_Inverse(const XrQuaternionf q) {
    XrQuaternionf r;
    r.x = -q.x;
    r.y = -q.y;
    r.z = -q.z;
    r.w = q.w;
    return r;
}

static inline XrVector3f XrQuaternionf_Rotate(const XrQuaternionf a, const XrVector3f v) {
    XrVector3f r;
    XrQuaternionf q = {v.x, v.y, v.z, 0.0f};
    XrQuaternionf aq;
    XrQuaternionf_Multiply(&aq, &q, &a);
    XrQuaternionf aInv = XrQuaternionf_Inverse(a);
    XrQuaternionf aqaInv;
    XrQuaternionf_Multiply(&aqaInv, &aInv, &aq);
    r.x = aqaInv.x;
    r.y = aqaInv.y;
    r.z = aqaInv.z;
    return r;
}

static inline XrPosef XrPosef_Identity() {
    XrPosef r;
    r.orientation = XrQuaternionf_Identity();
    r.position = XrVector3f_Zero();
    return r;
}

static inline XrPosef XrPosef_Create(const XrQuaternionf orientation, const XrVector3f position) {
    XrPosef r;
    r.orientation = orientation;
    r.position = position;
    return r;
}

static inline XrVector3f XrPosef_Transform(const XrPosef a, const XrVector3f v) {
    XrVector3f r0 = XrQuaternionf_Rotate(a.orientation, v);
    XrVector3f result;
    XrVector3f_Add(&result, &r0, &a.position);
    return result;
}

static inline XrPosef XrPosef_Inverse(const XrPosef a) {
    XrPosef b;
    b.orientation = XrQuaternionf_Inverse(a.orientation);
    b.position = XrQuaternionf_Rotate(b.orientation, XrVector3f_ScalarMultiply(a.position, -1.0f));
    return b;
}

// These functions were added to xr_linear.h in 1.0.27
// Note that they are named the same, but use parameters
// slightly differently. So if your compiler sent you here
// update usage to match xr_linear.h.
#if XR_CURRENT_API_VERSION < XR_MAKE_VERSION(1, 0, 27)

inline static void XrMatrix4x4f_CreateFromRigidTransform(XrMatrix4x4f* result, const XrPosef* s) {
    const XrVector3f identityScale = {1.0f, 1.0f, 1.0f};
    XrMatrix4x4f_CreateTranslationRotationScale(
        result, &s->position, &s->orientation, &identityScale);
}

inline static void XrQuaternionf_CreateIdentity(XrQuaternionf* q) {
    q->x = 0.0f;
    q->y = 0.0f;
    q->z = 0.0f;
    q->w = 1.0f;
}

inline static void XrQuaternionf_Invert(XrQuaternionf* result, const XrQuaternionf* q) {
    result->x = -q->x;
    result->y = -q->y;
    result->z = -q->z;
    result->w = q->w;
}

inline static void XrQuaternionf_Normalize(XrQuaternionf* q) {
    const float lengthRcp = XrRcpSqrt(q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w);
    q->x *= lengthRcp;
    q->y *= lengthRcp;
    q->z *= lengthRcp;
    q->w *= lengthRcp;
}

inline static void
XrQuaternionf_RotateVector3f(XrVector3f* result, const XrQuaternionf* a, const XrVector3f* v) {
    XrQuaternionf q = {v->x, v->y, v->z, 0.0f};
    XrQuaternionf aq;
    XrQuaternionf_Multiply(&aq, &q, a);
    XrQuaternionf aInv;
    XrQuaternionf_Invert(&aInv, a);
    XrQuaternionf aqaInv;
    XrQuaternionf_Multiply(&aqaInv, &aInv, &aq);

    result->x = aqaInv.x;
    result->y = aqaInv.y;
    result->z = aqaInv.z;
}

inline static void XrPosef_CreateIdentity(XrPosef* result) {
    XrQuaternionf_CreateIdentity(&result->orientation);
    XrVector3f_Set(&result->position, 0);
}

inline static void
XrPosef_TransformVector3f(XrVector3f* result, const XrPosef* a, const XrVector3f* v) {
    XrVector3f r0;
    XrQuaternionf_RotateVector3f(&r0, &a->orientation, v);
    XrVector3f_Add(result, &r0, &a->position);
}

inline static void XrPosef_Multiply(XrPosef* result, const XrPosef* a, const XrPosef* b) {
    XrQuaternionf_Multiply(&result->orientation, &b->orientation, &a->orientation);
    XrPosef_TransformVector3f(&result->position, a, &b->position);
}

inline static void XrPosef_Invert(XrPosef* result, const XrPosef* a) {
    XrQuaternionf_Invert(&result->orientation, &a->orientation);
    XrVector3f aPosNeg;
    XrVector3f_Scale(&aPosNeg, &a->position, -1.0f);
    XrQuaternionf_RotateVector3f(&result->position, &result->orientation, &aPosNeg);
}

// Creates a rotation matrix.
// If -Z=forward, +Y=up, +X=right, then radiansX=pitch, radiansY=yaw, radiansZ=roll.
static inline void XrMatrix4x4f_CreateRotationRadians(
    XrMatrix4x4f* result,
    const float radiansX,
    const float radiansY,
    const float radiansZ) {
    const float sinX = sinf(radiansX);
    const float cosX = cosf(radiansX);
    const XrMatrix4x4f rotationX = {
        {1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         cosX,
         sinX,
         0.0f,
         0.0f,
         -sinX,
         cosX,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f}};

    const float sinY = sinf(radiansY);
    const float cosY = cosf(radiansY);
    const XrMatrix4x4f rotationY = {
        {cosY,
         0,
         -sinY,
         0.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         sinY,
         0.0f,
         cosY,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f}};

    const float sinZ = sinf(radiansZ);
    const float cosZ = cosf(radiansZ);
    const XrMatrix4x4f rotationZ = {
        {cosZ,
         sinZ,
         0.0f,
         0.0f,
         -sinZ,
         cosZ,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f,
         0.0f,
         0.0f,
         1.0f}};

    XrMatrix4x4f rotationXY;
    XrMatrix4x4f_Multiply(&rotationXY, &rotationY, &rotationX);
    XrMatrix4x4f_Multiply(result, &rotationZ, &rotationXY);
}

#endif // XR_CURRENT_API_VERSION < XR_MAKE_VERSION(1, 0, 27)

#endif // OPENXR_OCULUS_HELPERS_H
