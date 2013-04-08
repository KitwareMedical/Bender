/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkQuaternion.txx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkDualQuaternion.h"

#ifndef __vtkDualQuaternion_txx
#define __vtkDualQuaternion_txx

#include <vtkMath.h>

#include <cmath>

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T>::vtkDualQuaternion()
  : Real() // identity
  , Dual(0.)
{
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T>
::vtkDualQuaternion(const T rd[8])
  : Real(rd[0], rd[1], rd[2], rd[3])
  , Dual(rd[4], rd[5], rd[6], rd[7])
{
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T>
::vtkDualQuaternion(const T& rw, const T& rx, const T& ry, const T& rz,
                    const T& dw, const T& dx, const T& dy, const T& dz)
  : Real(rw, rx, ry, rz)
  , Dual(dw, dx, dy, dz)
{
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T>
::vtkDualQuaternion(const vtkQuaternion<T>& real, const vtkQuaternion<T>& dual)
  : Real(real)
  , Dual(dual)
{
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T>
::vtkDualQuaternion(const vtkQuaternion<T>& rotation, const T translation[3])
{
  this->SetRotationTranslation(rotation, translation);
}

//----------------------------------------------------------------------------
template<typename T> vtkQuaternion<T> vtkDualQuaternion<T>::GetReal() const
{
  return this->Real;
}

//----------------------------------------------------------------------------
template<typename T> vtkQuaternion<T> vtkDualQuaternion<T>::GetDual() const
{
  return this->Dual;
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>::Invert()
{
  vtkDualQuaternion inverse = this->Inverse();
  this->operator=(this->Inverse());
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>::Inverse()const
{
  T realSquareNorm;
  T dualSquareNorm;
  this->ReciprocalLengthSquared(realSquareNorm, dualSquareNorm);
  return vtkDualQuaternion<T>(
    this->Real.GetW() * realSquareNorm,
    -this->Real.GetX() * realSquareNorm,
    -this->Real.GetY() * realSquareNorm,
    -this->Real.GetZ() * realSquareNorm,
    +this->Dual.GetW() * realSquareNorm + this->Real.GetW() * dualSquareNorm,
    -this->Dual.GetX() * realSquareNorm - this->Real.GetX() * dualSquareNorm,
    -this->Dual.GetY() * realSquareNorm - this->Real.GetY() * dualSquareNorm,
    -this->Dual.GetZ() * realSquareNorm - this->Real.GetZ() * dualSquareNorm);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>::Inverse2()const
{
  T sqr_len_0 = this->Real.SquaredNorm();
  T sqr_len_e = 2.0f * this->Real.Dot(this->Dual);
  T inv_sqr_len_0 = 1.0f / sqr_len_0;
  T inv_sqr_len_e = -sqr_len_e / (sqr_len_0 * sqr_len_0);
  vtkDualQuaternion<T> conj = this->Conjugated();
  return vtkDualQuaternion<T>(conj.Real*inv_sqr_len_0,
                              conj.Dual*inv_sqr_len_0  + conj.Real*inv_sqr_len_e);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>::Normalize()
{
  T length = 1. / this->Real.Norm();
  T dot  = this->Real.Dot(this->Dual);
  vtkQuaternion<T> a;
  a = this=->Real * (d * length * length);
  this->Dual = (this->Dual - a) * length;
  this->Real.Normalize();
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>::Conjugate()
{
  this->Real.Conjugate();
  this->Dual.Conjugate();
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>::Conjugated()const
{
  return vtkDualQuaternion<T>(this->Real.Conjugated(), this->Dual.Conjugated());
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::SetRotationTranslation(const vtkQuaternion<T>& rotation, const T position[3])
{
  vtkQuaternion<T> translation(
    -0.5f * (position[0] * rotation.GetX() + position[1] * rotation.GetY() + position[2] * rotation.GetZ()),
    0.5f * (position[0] * rotation.GetW() + position[1] * rotation.GetZ() - position[2] * rotation.GetY()),
    0.5f * (-position[0] * rotation.GetZ() + position[1] * rotation.GetW() + position[2] * rotation.GetX()),
    0.5f * (position[0] * rotation.GetY() - position[1] * rotation.GetX() + position[2] * rotation.GetW())
    );
  *this = vtkDualQuaternion<T>(rotation, translation);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::SetRotation(const vtkQuaternion<T>& rotation)
{
  double origin[3] = {0.,0.,0.};
  this->SetRotationPosition(rotation, origin);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::SetTranslation(const T translation[3])
{
  this->SetRotationPosition(vtkQuaternion<T>(), translation);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::GetPosition(T position[3])const
{
  const T real[4];
  real[0] = this->Real.GetW();
  real[1] = this->Real.GetX();
  real[2] = this->Real.GetY();
  real[3] = this->Real.GetZ();

  const T real2[4];
  real2[0] = 2. * real[0];
  real2[1] = 2. * real[1];
  real2[2] = 2. * real[2];
  real2[3] = 2. * real[3];

  const T dual[4];
  dual[0] = this->Dual.GetW();
  dual[1] = this->Dual.GetX();
  dual[2] = this->Dual.GetY();
  dual[3] = this->Dual.GetZ();

  position[0] = -dual[0] * real2[1] + real2[0] * dual[1] - dual[2] * real2[3] + real2[2] * dual[3];
  position[1] = -dual[0] * real2[2] + real2[3] * dual[1] + dual[2] * real2[0] - real2[1] * dual[3];
  position[2] = -dual[0] * real2[3] - real2[2] * dual[1] + dual[2] * real2[1] + real2[0] * dual[3];

  const T norm = this->Real.SquaredNorm();
  const T invNorm = 1. / norm;
  position[0] *= invNorm;
  position[0] *= invNorm;
  position[0] *= invNorm;
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::GetTranslation(T translation[3])const
{
  vtkQuaternion<T> q = ( this->Dual * 2.0f ) * this->Real.Conjugated();
  translation[0] = q.GetX();
  translation[1] = q.GetY();
  translation[2] = q.GetZ();
}

//----------------------------------------------------------------------------
template<typename T> bool vtkDualQuaternion<T>
::operator==(const vtkDualQuaternion<T>& dq)const
{
  return this->Real == dq.Real && this->Dual == dq.Dual;
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::operator+(const vtkDualQuaternion<T>& dq)const
{
  return vtkDualQuaternion<T>(this->Real + dq.Real, this->Dual + dq.Dual);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::operator-(const vtkDualQuaternion<T>& dq)const
{
  return vtkDualQuaternion<T>(this->Real - dq.Real, this->Dual - dq.Dual);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::operator-()const
{
  return vtkDualQuaternion<T>(-this->Real, -this->Dual);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::operator*(const T& scalar)const
{
  return vtkDualQuaternion<T>(this->Real * scalar, this->Dual * scalar);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::operator*(const vtkDualQuaternion<T>& dq)const
{
  return vtkDualQuaternion<T>(this->Real * dq.Real, this->Real * dq.Dual + this->Dual * dq.Real);
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::Lerp(const T& t, const vtkDualQuaternion<T>& _dq)const
{
  T dot = this->Real.Dot(_dq.Real);
  vtkDualQuaternion dq = dot >= 0. ? _dq : -_dq;

  vtkDualQuaternion<T> res;
  res = dq - *this;
  res = res * t;
  res = res + *this;
  return res;
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::ScLerp(const T& t, const vtkDualQuaternion<T>& _dq)const
{
  if (t == 0.)
    {
    return *this;
    }
  if (t == 1.)
    {
    return _dq;
    }

  T dot = this->Real.Dot(_dq.Real);
  vtkDualQuaternion dq = dot >= 0. ? _dq : -_dq;

  vtkDualQuaternion<T> i = this->Inverse();
  vtkDualQuaternion<T> d = i * dq;

  T range[2] = {-1., -1.};
  T cosine = d.Real.GetW();
  //vtkMath::ClampValue(d.Real.GetW(), range, &cosine);
  T theta = acos(cosine) * t;
  T sinTheta = sin(theta);
  T cosTheta = cos(theta);

  T axis[3];
  axis[0] = d.Real.GetX();
  axis[1] = d.Real.GetY();
  axis[2] = d.Real.GetZ();
  T l = axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2];
  T r = l < 1e-5f ? 1.0f : 1. / sqrt(l);
  T rs = r * sinTheta;
  vtkQuaternion<T> dd = d.Dual * rs;

  vtkMath::MultiplyScalar(axis, r);

  T u = (d.Real.GetW() * dd.GetW() - d.Dual.GetW() * t * cosTheta) * r;

  d = vtkDualQuaternion<T>(
    cosTheta,
    axis[0] * sinTheta,
    axis[1] * sinTheta,
    axis[2] * sinTheta,
    dd.GetW() * t,
    axis[0] * u + dd.GetX(),
    axis[1] * u + dd.GetY(),
    axis[2] * u + dd.GetZ()
    );

  return *this * d;
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::ScLerp2(const T& t, const vtkDualQuaternion<T>& _dq)const
{
  if (t == 0.)
    {
    return *this;
    }
  if (t == 1.)
    {
    return _dq;
    }

  // Make sure dot product is >= 0
  T dot = this->Real.Dot(_dq.Real);
  vtkDualQuaternion dq = dot >= 0. ? _dq : -_dq;

  vtkDualQuaternion<T> dif_dq = this->Inverse2();
  dif_dq = dif_dq * dq;
  //dif_dq.second = mul_dual(dif_dq.first, dif_dq.second, to_sign_corrected_real, to_sign_corrected_dual);
  //dif_dq.first = mul_real(dif_dq.first, to_sign_corrected_real);

  T angle, pitch;
  T direction[3];
  T moment[3];
  dif_dq.ToScrew(angle, pitch, direction, moment);

  angle *= t;
  pitch *= t;
  dif_dq.FromScrew(angle, pitch, direction, moment);

  //dif_dq.second = mul_dual(lhs_real, lhs_dual, dif_dq.first, dif_dq.second);
  //dif_dq.first = mul_real(lhs_real, dif_dq.first);
  dif_dq = *this * dif_dq;

  return dif_dq;
}

//----------------------------------------------------------------------------
template<typename T> vtkDualQuaternion<T> vtkDualQuaternion<T>
::Dot(const vtkDualQuaternion<T>& dq)const
{
  return *this * dq.SwapConjugate();
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::LengthSquared(T& real, T& dual)const
{
  real = this->Real.SquaredNorm();
  dual = 2. * this->Real.Dot(this->Dual);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::ReciprocalLengthSquared(T& real, T& dual)const
{
  real = 1. / this->Real.SquaredNorm();
  dual = -2. * this->Real.Dot(this->Dual) * real * real;
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::ToScrew(T& angle, T& pitch, T dir[3], T moment[3])const
{
  if (abs(this->Real.GetW()) >= 1.)
    {
    // pure translation

    angle = 0.;
    dir[0] = this->Dual.GetX();
    dir[1] = this->Dual.GetY();
    dir[2] = this->Dual.GetZ();

    T dir_sq_len = vtkMath::Dot(dir, dir);

    if (dir_sq_len > T(1e-6))
      {
      T dir_len = sqrt(dir_sq_len);
      pitch = 2 * dir_len;
      dir[0] /= dir_len;
      dir[1] /= dir_len;
      dir[2] /= dir_len;
      }
    else
      {
      pitch = 0.;
      }

    moment[0] = 0.;
    moment[1] = 0.;
    moment[2] = 0.;
    }
  else
    {
    angle = 2 * acos(this->Real.GetW());
    T v[3];
    v[0] = this->Real.GetX();
    v[1] = this->Real.GetY();
    v[2] = this->Real.GetZ();
    T s = vtkMath::Dot(v, v);
    if (s < T(1e-6))
      {
      dir[0] = 0.;
      dir[1] = 0.;
      dir[2] = 0.;
      pitch = 0.;
      moment[0] = 0.;
      moment[1] = 0.;
      moment[2] = 0.;
      }
    else
      {
      T oos = 1. / sqrt(s);
      dir[0] = this->Real.GetX() * oos;
      dir[1] = this->Real.GetY() * oos;
      dir[2] = this->Real.GetZ() * oos;
      pitch = -2 * this->Dual.GetW() * oos;
      moment[0] = (this->Dual.GetX() - dir[0] * pitch * this->Real.GetW() * T(0.5)) * oos;
      moment[1] = (this->Dual.GetY() - dir[1] * pitch * this->Real.GetW() * T(0.5)) * oos;
      moment[2] = (this->Dual.GetZ() - dir[2] * pitch * this->Real.GetW() * T(0.5)) * oos;
      }
    }
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::FromScrew(T angle, T pitch, T dir[3], T moment[3])
{
  T sa = sin(angle * T(0.5));
  T ca = cos(angle * T(0.5));
  *this = vtkDualQuaternion<T>(ca, dir[0] * sa, dir[1] * sa, dir[2] * sa,
                               -pitch * sa * T(0.5),
                               sa * moment[0] + T(0.5) * pitch * ca * dir[0],
                               sa * moment[1] + T(0.5) * pitch * ca * dir[1],
                               sa * moment[2] + T(0.5) * pitch * ca * dir[2]);
}

//----------------------------------------------------------------------------
template<typename T> void vtkDualQuaternion<T>
::TransformPoint(const T point[3], T transformedPoint[3])const
{
  // t = p + 2a + 2b
  // a = rv x ( (rv x p)+ p*w)
  // b = (dv * w) - (rv * w) + (rv x dv)

  T rv[3];
  rv[0] = this->Real.GetX();
  rv[1] = this->Real.GetY();
  rv[2] = this->Real.GetZ();
  T dv[3];
  dv[0] = this->Dual.GetX();
  dv[1] = this->Dual.GetY();
  dv[2] = this->Dual.GetZ();

  T rvXp[3];
  vtkMath::Cross(rv, point, rvXp);
  T rvXpPwrv[3];
  rvXpPwrv[0] = rvXp[0] + this->Real.GetW()*point[0];
  rvXpPwrv[1] = rvXp[1] + this->Real.GetW()*point[1];
  rvXpPwrv[2] = rvXp[2] + this->Real.GetW()*point[2];
  T a[3];
  vtkMath::Cross(rv, rvXpPwrv, a);
  T rvXdv[3];
  vtkMath::Cross(rv, dv, rvXdv);
  T b[3];
  b[0] = this->Real.GetW() * dv[0] - this->Dual.GetW() * rv[0] + rvXdv[0];
  b[1] = this->Real.GetW() * dv[1] - this->Dual.GetW() * rv[1] + rvXdv[1];
  b[2] = this->Real.GetW() * dv[2] - this->Dual.GetW() * rv[2] + rvXdv[2];
  transformedPoint[0] = point[0] + 2. * a[0] + 2. * b[0];
  transformedPoint[1] = point[1] + 2. * a[1] + 2. * b[1];
  transformedPoint[2] = point[2] + 2. * a[2] + 2. * b[2];
}

#endif //__vtkDualQuaternion_txx
