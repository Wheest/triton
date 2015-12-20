/*
 * Copyright (c) 2015, PHILIPPE TILLET. All rights reserved.
 *
 * This file is part of ISAAC.
 *
 * ISAAC is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */
#define NOMINMAX

#include <cassert>
#include <algorithm>
#include <stdexcept>

#include "isaac/array.h"
#include "isaac/tuple.h"
#include "isaac/exception/unknown_datatype.h"
#include "isaac/profiles/profiles.h"
#include "isaac/symbolic/execute.h"
#include "isaac/symbolic/io.h"

namespace isaac
{

/*--- Constructors ---*/
//1D Constructors

int_t array_base::dsize()
{
  return std::max((int_t)1, shape_.prod()*size_of(dtype_));
}

array_base::array_base(int_t shape0, numeric_type dtype, driver::Context const & context) :
  dtype_(dtype), shape_{shape0}, start_(0), stride_(1),
  context_(context), data_(context_, dsize()),
  T(isaac::trans(*this))
{ }

array_base::array_base(int_t shape0, numeric_type dtype, driver::Buffer data, int_t start, int_t inc):
  dtype_(dtype), shape_{shape0}, start_(start), stride_(inc), context_(data.context()), data_(data),
  T(isaac::trans(*this))
{ }


template<class DT>
array_base::array_base(std::vector<DT> const & x, driver::Context const & context):
  dtype_(to_numeric_type<DT>::value), shape_{(int_t)x.size()}, start_(0), stride_(1),
  context_(context), data_(context, dsize()),
  T(isaac::trans(*this))
{ *this = x; }

array_base::array_base(array_base & v, slice const & s0) :
  dtype_(v.dtype_), shape_{s0.size(v.shape_[0])}, start_(v.start_ + v.stride_[0]*s0.start), stride_(v.stride_[0]*s0.stride), context_(v.context()), data_(v.data_),
  T(isaac::trans(*this))
{}

#define INSTANTIATE(T) template ISAACAPI array_base::array_base(std::vector<T> const &, driver::Context const &)
INSTANTIATE(char);
INSTANTIATE(unsigned char);
INSTANTIATE(short);
INSTANTIATE(unsigned short);
INSTANTIATE(int);
INSTANTIATE(unsigned int);
INSTANTIATE(long);
INSTANTIATE(unsigned long);
INSTANTIATE(long long);
INSTANTIATE(unsigned long long);
INSTANTIATE(float);
INSTANTIATE(double);
#undef INSTANTIATE

// 2D
array_base::array_base(int_t shape0, int_t shape1, numeric_type dtype, driver::Context const & context) :
  dtype_(dtype), shape_{shape0, shape1}, start_(0), stride_(1,shape0),
  context_(context), data_(context_, dsize()),
  T(isaac::trans(*this))
{}

array_base::array_base(int_t shape0, int_t shape1, numeric_type dtype, driver::Buffer data, int_t start, int_t ld) :
  dtype_(dtype), shape_{shape0, shape1}, start_(start), stride_(1, ld), context_(data.context()), data_(data),
  T(isaac::trans(*this))
{ }

array_base::array_base(array_base & M, slice const & s0, slice const & s1) :
  dtype_(M.dtype_), shape_{s0.size(M.shape_[0]), s1.size(M.shape_[1])},
  start_(M.start_ + M.stride_[0]*s0.start + s1.start*M.stride_[1]),
  stride_(M.stride_[0]*s0.stride, M.stride_[1]*s1.stride),
  context_(M.data_.context()), data_(M.data_),
  T(isaac::trans(*this))
{ }


template<typename DT>
array_base::array_base(int_t shape0, int_t shape1, std::vector<DT> const & data, driver::Context const & context)
  : dtype_(to_numeric_type<DT>::value),
    shape_{shape0, shape1}, start_(0), stride_(1, shape0),
    context_(context), data_(context_, dsize()),
    T(isaac::trans(*this))
{
  isaac::copy(data, *this);
}

// 3D
array_base::array_base(int_t shape0, int_t shape1, int_t shape2, numeric_type dtype, driver::Context const & context) :
  dtype_(dtype), shape_{shape0, shape1, shape2}, start_(0), stride_(1, shape0),
  context_(context), data_(context_, dsize()),
  T(isaac::trans(*this))
{}

#define INSTANTIATE(T) template ISAACAPI array_base::array_base(int_t, int_t, std::vector<T> const &, driver::Context const &)
INSTANTIATE(char);
INSTANTIATE(unsigned char);
INSTANTIATE(short);
INSTANTIATE(unsigned short);
INSTANTIATE(int);
INSTANTIATE(unsigned int);
INSTANTIATE(long);
INSTANTIATE(unsigned long);
INSTANTIATE(long long);
INSTANTIATE(unsigned long long);
INSTANTIATE(float);
INSTANTIATE(double);
#undef INSTANTIATE


array_base::array_base(numeric_type dtype, shape_t const & shape, int_t start, shape_t const & stride, driver::Context const & context) :
    dtype_(dtype), shape_(shape), start_(start), stride_(stride), context_(context), data_(context_, dsize()),
    T(isaac::trans(*this))
{}

array_base::array_base(numeric_type dtype, shape_t const & shape, driver::Context const & context) : array_base(dtype, shape, 0, {1, shape[0]}, context)
{}

array_base::array_base(execution_handler const & other) :
  dtype_(other.x().dtype()),
  shape_(other.x().shape()), start_(0), stride_(1, shape_[0]),
  context_(other.x().context()), data_(context_, dsize()),
  T(isaac::trans(*this))
{
  *this = other;
}

//Destructor
array_base::~array_base()
{}

/*--- Getters ---*/
numeric_type array_base::dtype() const
{ return dtype_; }

shape_t const & array_base::shape() const
{ return shape_; }

int_t array_base::dim() const
{ return (int_t)shape_.size(); }

int_t array_base::start() const
{ return start_; }

shape_t const & array_base::stride() const
{ return stride_; }

driver::Context const & array_base::context() const
{ return context_; }

driver::Buffer const & array_base::data() const
{ return data_; }

driver::Buffer & array_base::data()
{ return data_; }


/*--- Assignment Operators ----*/
//---------------------------------------

array_base & array_base::operator=(array_base const & rhs)
{
    if(shape_.min()==0) return *this;
    assert(dtype_ == rhs.dtype());
    expression_tree expression(*this, rhs, op_element(BINARY_TYPE_FAMILY, ASSIGN_TYPE), context_, dtype_, shape_);
    execute(execution_handler(expression));
    return *this;
}

array_base & array_base::operator=(value_scalar const & rhs)
{
    if(shape_.min()==0) return *this;
    assert(dtype_ == rhs.dtype());
    expression_tree expression(*this, rhs, op_element(BINARY_TYPE_FAMILY, ASSIGN_TYPE), context_, dtype_, shape_);
    execute(execution_handler(expression));
    return *this;
}


array_base& array_base::operator=(execution_handler const & c)
{
  if(shape_.min()==0) return *this;
  assert(dtype_ == c.x().dtype());
  expression_tree expression(*this, c.x(), op_element(BINARY_TYPE_FAMILY, ASSIGN_TYPE), context_, dtype_, shape_);
  execute(execution_handler(expression, c.execution_options(), c.dispatcher_options(), c.compilation_options()));
  return *this;
}

array_base & array_base::operator=(expression_tree const & rhs)
{
  return *this = execution_handler(rhs);
}


template<class DT>
array_base & array_base::operator=(std::vector<DT> const & rhs)
{
  assert(dim()<=1);
  isaac::copy(rhs, *this);
  return *this;
}

#define INSTANTIATE(TYPE) template ISAACAPI array_base& array_base::operator=<TYPE>(std::vector<TYPE> const &)

INSTANTIATE(char);
INSTANTIATE(unsigned char);
INSTANTIATE(short);
INSTANTIATE(unsigned short);
INSTANTIATE(int);
INSTANTIATE(unsigned int);
INSTANTIATE(long);
INSTANTIATE(unsigned long);
INSTANTIATE(long long);
INSTANTIATE(unsigned long long);
INSTANTIATE(float);
INSTANTIATE(double);
#undef INSTANTIATE





expression_tree array_base::operator-()
{ return expression_tree(*this, invalid_node(), op_element(UNARY_TYPE_FAMILY, SUB_TYPE), context_, dtype_, shape_); }

expression_tree array_base::operator!()
{ return expression_tree(*this, invalid_node(), op_element(UNARY_TYPE_FAMILY, NEGATE_TYPE), context_, INT_TYPE, shape_); }

//
array_base & array_base::operator+=(value_scalar const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, ADD_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator+=(array_base const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, ADD_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator+=(expression_tree const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, ADD_TYPE), rhs.context(), dtype_, shape_); }
//----
array_base & array_base::operator-=(value_scalar const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, SUB_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator-=(array_base const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, SUB_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator-=(expression_tree const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, SUB_TYPE), rhs.context(), dtype_, shape_); }
//----
array_base & array_base::operator*=(value_scalar const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, MULT_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator*=(array_base const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, MULT_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator*=(expression_tree const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, MULT_TYPE), rhs.context(), dtype_, shape_); }
//----
array_base & array_base::operator/=(value_scalar const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, DIV_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator/=(array_base const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, DIV_TYPE), context_, dtype_, shape_); }

array_base & array_base::operator/=(expression_tree const & rhs)
{ return *this = expression_tree(*this, rhs, op_element(BINARY_TYPE_FAMILY, DIV_TYPE), rhs.context(), dtype_, shape_); }

/*--- Indexing operators -----*/
//---------------------------------------
expression_tree array_base::operator[](for_idx_t idx) const
{
  return expression_tree(*this, idx, op_element(BINARY_TYPE_FAMILY, ACCESS_INDEX_TYPE), context_, dtype_, {1});
}

scalar array_base::operator [](int_t idx)
{
  assert(dim()<=1);
  return scalar(dtype_, data_, start_ + idx);
}

const scalar array_base::operator [](int_t idx) const
{
  assert(dim()<=1);
  return scalar(dtype_, data_, start_ + idx);
}

view array_base::operator[](slice const & e1)
{
  assert(dim()<=1);
  return view(*this, e1);
}

view array_base::operator()(int_t i, int_t j)
{
  assert(dim()==2 && "Too many indices in array");
  return view(1, dtype_, data_, start_ + i*stride_[0] + j*stride_[1], 1);
}

view array_base::operator()(int_t i, slice const & sj)
{
  assert(dim()==2 && "Too many indices in array");
  return view(sj.size(shape_[1]), dtype_, data_, start_ + i*stride_[0] + sj.start*stride_[1], sj.stride*stride_[1]);
}


view array_base::operator()(slice const & si, int_t j)
{
  assert(dim()==2 && "Too many indices in array");
  return view(si.size(shape_[0]), dtype_, data_, start_ + si.start*stride_[0] + j*stride_[1], si.stride);
}


view array_base::operator()(slice const & si, slice const & sj)
{
  assert(dim()==2 && "Too many indices in array");
  return view(*this, si, sj);
}

//---------------------------------------
/*--- array ---*/

array::array(expression_tree const & proxy) : array_base(execution_handler(proxy)) {}

array::array(array_base const & other): array_base(other.dtype(), other.shape(), other.context())
{ *this = other; }

array::array(array const &other): array((array_base const &)other)
{ }


//---------------------------------------
/*--- View ---*/
view::view(array & data) : array_base(data){}
view::view(array_base& data, slice const & s1) : array_base(data, s1) {}
view::view(array_base& data, slice const & s1, slice const & s2) : array_base(data, s1, s2) {}
view::view(int_t size1, numeric_type dtype, driver::Buffer data, int_t start, int_t inc) : array_base(size1, dtype, data, start, inc) {}


//---------------------------------------
/*--- Scalar ---*/
namespace detail
{

template<class T>
void copy(driver::Context const & context, driver::Buffer const & data, T value)
{
  driver::backend::queues::get(context,0).write(data, CL_TRUE, 0, sizeof(T), (void*)&value);
}

}

scalar::scalar(numeric_type dtype, const driver::Buffer &data, int_t offset): array_base(1, dtype, data, offset, 1)
{ }

scalar::scalar(value_scalar value, driver::Context const & context) : array_base(1, value.dtype(), context)
{
  switch(dtype_)
  {
    case CHAR_TYPE: detail::copy(context_, data_, (char)value); break;
    case UCHAR_TYPE: detail::copy(context_, data_, (unsigned char)value); break;
    case SHORT_TYPE: detail::copy(context_, data_, (short)value); break;
    case USHORT_TYPE: detail::copy(context_, data_, (unsigned short)value); break;
    case INT_TYPE: detail::copy(context_, data_, (int)value); break;
    case UINT_TYPE: detail::copy(context_, data_, (unsigned int)value); break;
    case LONG_TYPE: detail::copy(context_, data_, (long)value); break;
    case ULONG_TYPE: detail::copy(context_, data_, (unsigned long)value); break;
    case FLOAT_TYPE: detail::copy(context_, data_, (float)value); break;
    case DOUBLE_TYPE: detail::copy(context_, data_, (double)value); break;
    default: throw unknown_datatype(dtype_);
  }
}


scalar::scalar(numeric_type dtype, driver::Context const & context) : array_base(1, dtype, context)
{ }

scalar::scalar(expression_tree const & proxy) : array_base(proxy){ }

void scalar::inject(values_holder & v) const
{
    int_t dtsize = size_of(dtype_);
  #define HANDLE_CASE(DTYPE, VAL) \
  case DTYPE:\
    driver::backend::queues::get(context_, 0).read(data_, CL_TRUE, start_*dtsize, dtsize, (void*)&v.VAL); break;\

    switch(dtype_)
    {
      HANDLE_CASE(CHAR_TYPE, int8);
      HANDLE_CASE(UCHAR_TYPE, uint8);
      HANDLE_CASE(SHORT_TYPE, int16);
      HANDLE_CASE(USHORT_TYPE, uint16);
      HANDLE_CASE(INT_TYPE, int32);
      HANDLE_CASE(UINT_TYPE, uint32);
      HANDLE_CASE(LONG_TYPE, int64);
      HANDLE_CASE(ULONG_TYPE, uint64);
      HANDLE_CASE(FLOAT_TYPE, float32);
      HANDLE_CASE(DOUBLE_TYPE, float64);
      default: throw unknown_datatype(dtype_);
    }
  #undef HANDLE_CASE
}

template<class TYPE>
TYPE scalar::cast() const
{
  values_holder v;
  inject(v);

#define HANDLE_CASE(DTYPE, VAL) case DTYPE: return static_cast<TYPE>(v.VAL)

  switch(dtype_)
  {
    HANDLE_CASE(CHAR_TYPE, int8);
    HANDLE_CASE(UCHAR_TYPE, uint8);
    HANDLE_CASE(SHORT_TYPE, int16);
    HANDLE_CASE(USHORT_TYPE, uint16);
    HANDLE_CASE(INT_TYPE, int32);
    HANDLE_CASE(UINT_TYPE, uint32);
    HANDLE_CASE(LONG_TYPE, int64);
    HANDLE_CASE(ULONG_TYPE, uint64);
    HANDLE_CASE(FLOAT_TYPE, float32);
    HANDLE_CASE(DOUBLE_TYPE, float64);
    default: throw unknown_datatype(dtype_);
  }
#undef HANDLE_CASE

}

scalar& scalar::operator=(value_scalar const & s)
{
  driver::CommandQueue& queue = driver::backend::queues::get(context_, 0);
  int_t dtsize = size_of(dtype_);

#define HANDLE_CASE(TYPE, CLTYPE) case TYPE:\
                            {\
                              CLTYPE v = s;\
                              queue.write(data_, CL_TRUE, start_*dtsize, dtsize, (void*)&v);\
                              return *this;\
                            }
  switch(dtype_)
  {
    HANDLE_CASE(CHAR_TYPE, char)
    HANDLE_CASE(UCHAR_TYPE, unsigned char)
    HANDLE_CASE(SHORT_TYPE, short)
    HANDLE_CASE(USHORT_TYPE, unsigned short)
    HANDLE_CASE(INT_TYPE, int)
    HANDLE_CASE(UINT_TYPE, unsigned int)
    HANDLE_CASE(LONG_TYPE, long)
    HANDLE_CASE(ULONG_TYPE, unsigned long)
    HANDLE_CASE(FLOAT_TYPE, float)
    HANDLE_CASE(DOUBLE_TYPE, double)
    default: throw unknown_datatype(dtype_);
  }
}

#define INSTANTIATE(type) scalar::operator type() const { return cast<type>(); }
  INSTANTIATE(char)
  INSTANTIATE(unsigned char)
  INSTANTIATE(short)
  INSTANTIATE(unsigned short)
  INSTANTIATE(int)
  INSTANTIATE(unsigned int)
  INSTANTIATE(long)
  INSTANTIATE(unsigned long)
  INSTANTIATE(long long)
  INSTANTIATE(unsigned long long)
  INSTANTIATE(float)
  INSTANTIATE(double)
#undef INSTANTIATE

std::ostream & operator<<(std::ostream & os, scalar const & s)
{
  switch(s.dtype())
  {
//    case BOOL_TYPE: return os << static_cast<bool>(s);
    case CHAR_TYPE: return os << static_cast<char>(s);
    case UCHAR_TYPE: return os << static_cast<unsigned char>(s);
    case SHORT_TYPE: return os << static_cast<short>(s);
    case USHORT_TYPE: return os << static_cast<unsigned short>(s);
    case INT_TYPE: return os << static_cast<int>(s);
    case UINT_TYPE: return os << static_cast<unsigned int>(s);
    case LONG_TYPE: return os << static_cast<long>(s);
    case ULONG_TYPE: return os << static_cast<unsigned long>(s);
//    case HALF_TYPE: return os << static_cast<half>(s);
    case FLOAT_TYPE: return os << static_cast<float>(s);
    case DOUBLE_TYPE: return os << static_cast<double>(s);
    default: throw unknown_datatype(s.dtype());
  }
}

/*--- Binary Operators ----*/
//-----------------------------------
shape_t broadcast(shape_t const & a, shape_t const & b)
{
    std::vector<int_t> aa = a, bb = b, result;
    size_t as = aa.size(), bs = bb.size();
    if(as < bs)
        aa.insert(aa.begin(), bs - as, 1);
    else
        bb.insert(bb.begin(), as - bs, 1);
    for(size_t i = 0 ; i < std::max(as, bs) ; ++i){
        assert((aa[i] == bb[i] || aa[i]==1 || bb[i]==1) && "Cannot broadcast");
        result.push_back(std::max(aa[i], bb[i]));
    }
    return shape_t(result);
}

#define DEFINE_ELEMENT_BINARY_OPERATOR(OP, OPNAME, DTYPE) \
expression_tree OPNAME (array_base const & x, expression_tree const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, broadcast(x.shape(), y.shape())); } \
\
expression_tree OPNAME (array_base const & x, array_base const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, broadcast(x.shape(), y.shape())); }\
\
expression_tree OPNAME (array_base const & x, value_scalar const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); }\
\
expression_tree OPNAME (array_base const & x, for_idx_t const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); }\
\
\
expression_tree OPNAME (expression_tree const & x, expression_tree const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, broadcast(x.shape(), y.shape())); } \
 \
expression_tree OPNAME (expression_tree const & x, array_base const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, broadcast(x.shape(), y.shape())); } \
\
expression_tree OPNAME (expression_tree const & x, value_scalar const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); } \
\
expression_tree OPNAME (expression_tree const & x, for_idx_t const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); } \
\
\
expression_tree OPNAME (value_scalar const & y, expression_tree const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); } \
\
expression_tree OPNAME (value_scalar const & y, array_base const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); }\
\
expression_tree OPNAME (value_scalar const & x, for_idx_t const & y) \
{ return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OP), DTYPE); }\
\
\
expression_tree OPNAME (for_idx_t const & y, expression_tree const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); } \
\
expression_tree OPNAME (for_idx_t const & y, value_scalar const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP), DTYPE); } \
\
expression_tree OPNAME (for_idx_t const & y, array_base const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP), x.context(), DTYPE, x.shape()); }\
\
expression_tree OPNAME (for_idx_t const & y, for_idx_t const & x) \
{ return expression_tree(y, x, op_element(BINARY_TYPE_FAMILY, OP)); }


DEFINE_ELEMENT_BINARY_OPERATOR(ADD_TYPE, operator +, x.dtype())
DEFINE_ELEMENT_BINARY_OPERATOR(SUB_TYPE, operator -, x.dtype())
DEFINE_ELEMENT_BINARY_OPERATOR(MULT_TYPE, operator *, x.dtype())
DEFINE_ELEMENT_BINARY_OPERATOR(DIV_TYPE, operator /, x.dtype())

DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_MAX_TYPE, maximum, x.dtype())
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_MIN_TYPE, minimum, x.dtype())
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_POW_TYPE, pow, x.dtype())

DEFINE_ELEMENT_BINARY_OPERATOR(ASSIGN_TYPE, assign, x.dtype())


DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_GREATER_TYPE, operator >, INT_TYPE)
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_GEQ_TYPE, operator >=, INT_TYPE)
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_LESS_TYPE, operator <, INT_TYPE)
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_LEQ_TYPE, operator <=, INT_TYPE)
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_EQ_TYPE, operator ==, INT_TYPE)
DEFINE_ELEMENT_BINARY_OPERATOR(ELEMENT_NEQ_TYPE, operator !=, INT_TYPE)

#define DEFINE_OUTER(LTYPE, RTYPE) \
expression_tree outer(LTYPE const & x, RTYPE const & y)\
{\
    assert(x.dim()<=1 && y.dim()<=1);\
    if(x.dim()<1 || y.dim()<1)\
      return x*y;\
    return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OUTER_PROD_TYPE), x.context(), x.dtype(), {x.shape().max(), y.shape().max()} );\
}\

DEFINE_OUTER(array_base, array_base)
DEFINE_OUTER(expression_tree, array_base)
DEFINE_OUTER(array_base, expression_tree)
DEFINE_OUTER(expression_tree, expression_tree)

#undef DEFINE_ELEMENT_BINARY_OPERATOR

#define DEFINE_ROT(LTYPE, RTYPE, CTYPE, STYPE)\
expression_tree rot(LTYPE const & x, RTYPE const & y, CTYPE const & c, STYPE const & s)\
{ return fuse(assign(x, c*x + s*y), assign(y, c*y - s*x)); }

DEFINE_ROT(array_base, array_base, scalar, scalar)
DEFINE_ROT(expression_tree, array_base, scalar, scalar)
DEFINE_ROT(array_base, expression_tree, scalar, scalar)
DEFINE_ROT(expression_tree, expression_tree, scalar, scalar)

DEFINE_ROT(array_base, array_base, value_scalar, value_scalar)
DEFINE_ROT(expression_tree, array_base, value_scalar, value_scalar)
DEFINE_ROT(array_base, expression_tree, value_scalar, value_scalar)
DEFINE_ROT(expression_tree, expression_tree, value_scalar, value_scalar)

DEFINE_ROT(array_base, array_base, expression_tree, expression_tree)
DEFINE_ROT(expression_tree, array_base, expression_tree, expression_tree)
DEFINE_ROT(array_base, expression_tree, expression_tree, expression_tree)
DEFINE_ROT(expression_tree, expression_tree, expression_tree, expression_tree)



//---------------------------------------

/*--- Math Operators----*/
//---------------------------------------
#define DEFINE_ELEMENT_UNARY_OPERATOR(OP, OPNAME) \
expression_tree OPNAME (array_base  const & x) \
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, OP), x.context(), x.dtype(), x.shape()); }\
\
expression_tree OPNAME (expression_tree const & x) \
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, OP), x.context(), x.dtype(), x.shape()); }

DEFINE_ELEMENT_UNARY_OPERATOR((x.dtype()==FLOAT_TYPE || x.dtype()==DOUBLE_TYPE)?FABS_TYPE:ABS_TYPE,  abs)
DEFINE_ELEMENT_UNARY_OPERATOR(ACOS_TYPE, acos)
DEFINE_ELEMENT_UNARY_OPERATOR(ASIN_TYPE, asin)
DEFINE_ELEMENT_UNARY_OPERATOR(ATAN_TYPE, atan)
DEFINE_ELEMENT_UNARY_OPERATOR(CEIL_TYPE, ceil)
DEFINE_ELEMENT_UNARY_OPERATOR(COS_TYPE,  cos)
DEFINE_ELEMENT_UNARY_OPERATOR(COSH_TYPE, cosh)
DEFINE_ELEMENT_UNARY_OPERATOR(EXP_TYPE,  exp)
DEFINE_ELEMENT_UNARY_OPERATOR(FLOOR_TYPE, floor)
DEFINE_ELEMENT_UNARY_OPERATOR(LOG_TYPE,  log)
DEFINE_ELEMENT_UNARY_OPERATOR(LOG10_TYPE,log10)
DEFINE_ELEMENT_UNARY_OPERATOR(SIN_TYPE,  sin)
DEFINE_ELEMENT_UNARY_OPERATOR(SINH_TYPE, sinh)
DEFINE_ELEMENT_UNARY_OPERATOR(SQRT_TYPE, sqrt)
DEFINE_ELEMENT_UNARY_OPERATOR(TAN_TYPE,  tan)
DEFINE_ELEMENT_UNARY_OPERATOR(TANH_TYPE, tanh)
#undef DEFINE_ELEMENT_UNARY_OPERATOR
//---------------------------------------


///*--- Misc----*/
////---------------------------------------
inline operation_type casted(numeric_type dtype)
{
  switch(dtype)
  {
//    case BOOL_TYPE: return CAST_BOOL_TYPE;
    case CHAR_TYPE: return CAST_CHAR_TYPE;
    case UCHAR_TYPE: return CAST_UCHAR_TYPE;
    case SHORT_TYPE: return CAST_SHORT_TYPE;
    case USHORT_TYPE: return CAST_USHORT_TYPE;
    case INT_TYPE: return CAST_INT_TYPE;
    case UINT_TYPE: return CAST_UINT_TYPE;
    case LONG_TYPE: return CAST_LONG_TYPE;
    case ULONG_TYPE: return CAST_ULONG_TYPE;
//    case FLOAT_TYPE: return CAST_HALF_TYPE;
    case FLOAT_TYPE: return CAST_FLOAT_TYPE;
    case DOUBLE_TYPE: return CAST_DOUBLE_TYPE;
    default: throw unknown_datatype(dtype);
  }
}

expression_tree cast(array_base const & x, numeric_type dtype)
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, casted(dtype)), x.context(), dtype, x.shape()); }

expression_tree cast(expression_tree const & x, numeric_type dtype)
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, casted(dtype)), x.context(), dtype, x.shape()); }

isaac::expression_tree eye(int_t M, int_t N, isaac::numeric_type dtype, driver::Context const & ctx)
{ return expression_tree(value_scalar(1), value_scalar(0), op_element(UNARY_TYPE_FAMILY, VDIAG_TYPE), ctx, dtype, {M, N}); }

array diag(array_base & x, int offset)
{
  assert(x.dim()==2 && "Input must be 2-d");
  int_t offi = -(offset<0)*offset, offj = (offset>0)*offset;
  int_t size = std::min(x.shape()[0] - offi, x.shape()[1] - offj);
  int_t start = offi + x.stride()[1]*offj;
  return array(size, x.dtype(), x.data(), start, x.stride()[1]+1);
}


isaac::expression_tree zeros(int_t M, int_t N, isaac::numeric_type dtype, driver::Context  const & ctx)
{ return expression_tree(value_scalar(0, dtype), invalid_node(), op_element(UNARY_TYPE_FAMILY, ADD_TYPE), ctx, dtype, {M, N}); }

inline shape_t flip(shape_t const & shape)
{
  shape_t res = shape;
  for(size_t i = 0 ; i < shape.size() ; ++i)
    res[i] = shape[(i + 1)%shape.size()];
  return res;
}

//inline size4 prod(size4 const & shape1, size4 const & shape2)
//{ return size4(shape1[0]*shape2[0], shape1[1]*shape2[1]);}

expression_tree trans(array_base  const & x) \
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, TRANS_TYPE), x.context(), x.dtype(), flip(x.shape())); }\
\
expression_tree trans(expression_tree const & x) \
{ return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, TRANS_TYPE), x.context(), x.dtype(), flip(x.shape())); }

expression_tree repmat(array_base const & A, int_t const & rep1, int_t const & rep2)
{
  int_t sub1 = A.shape()[0];
  int_t sub2 = A.dim()==2?A.shape()[1]:1;
  return expression_tree(A, make_tuple(A.context(), rep1, rep2, sub1, sub2), op_element(BINARY_TYPE_FAMILY, REPEAT_TYPE), A.context(), A.dtype(), {rep1*sub1, rep2*sub2});
}

expression_tree repmat(expression_tree const & A, int_t const & rep1, int_t const & rep2)
{
  int_t sub1 = A.shape()[0];
  int_t sub2 = A.dim()==2?A.shape()[1]:1;
  return expression_tree(A, make_tuple(A.context(), rep1, rep2, sub1, sub2), op_element(BINARY_TYPE_FAMILY, REPEAT_TYPE), A.context(), A.dtype(), {rep1*sub1, rep2*sub2});
}

#define DEFINE_ACCESS_ROW(TYPEA, TYPEB) \
  expression_tree row(TYPEA const & x, TYPEB const & i)\
  { return expression_tree(x, i, op_element(UNARY_TYPE_FAMILY, MATRIX_ROW_TYPE), x.context(), x.dtype(), {x.shape()[1]}); }

DEFINE_ACCESS_ROW(array_base, value_scalar)
DEFINE_ACCESS_ROW(array_base, for_idx_t)
DEFINE_ACCESS_ROW(array_base, expression_tree)

DEFINE_ACCESS_ROW(expression_tree, value_scalar)
DEFINE_ACCESS_ROW(expression_tree, for_idx_t)
DEFINE_ACCESS_ROW(expression_tree, expression_tree)

#define DEFINE_ACCESS_COL(TYPEA, TYPEB) \
  expression_tree col(TYPEA const & x, TYPEB const & i)\
  { return expression_tree(x, i, op_element(UNARY_TYPE_FAMILY, MATRIX_COLUMN_TYPE), x.context(), x.dtype(), {x.shape()[0]}); }

DEFINE_ACCESS_COL(array_base, value_scalar)
DEFINE_ACCESS_COL(array_base, for_idx_t)
DEFINE_ACCESS_COL(array_base, expression_tree)

DEFINE_ACCESS_COL(expression_tree, value_scalar)
DEFINE_ACCESS_COL(expression_tree, for_idx_t)
DEFINE_ACCESS_COL(expression_tree, expression_tree)

////---------------------------------------

///*--- Reductions ---*/
////---------------------------------------
#define DEFINE_REDUCTION(OP, OPNAME)\
expression_tree OPNAME(array_base const & x, int_t axis)\
{\
  if(axis < -1 || axis > x.dim())\
    throw std::out_of_range("The axis entry is out of bounds");\
  else if(axis==-1)\
    return expression_tree(x, invalid_node(), op_element(VECTOR_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {1});\
  else if(axis==0)\
    return expression_tree(x, invalid_node(), op_element(COLUMNS_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {x.shape()[1]});\
  else\
    return expression_tree(x, invalid_node(), op_element(ROWS_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {x.shape()[0]});\
}\
\
expression_tree OPNAME(expression_tree const & x, int_t axis)\
{\
  if(axis < -1 || axis > x.dim())\
    throw std::out_of_range("The axis entry is out of bounds");\
  if(axis==-1)\
    return expression_tree(x, invalid_node(), op_element(VECTOR_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {1});\
  else if(axis==0)\
    return expression_tree(x, invalid_node(), op_element(COLUMNS_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {x.shape()[1]});\
  else\
    return expression_tree(x, invalid_node(), op_element(ROWS_DOT_TYPE_FAMILY, OP), x.context(), x.dtype(), {x.shape()[0]});\
}

DEFINE_REDUCTION(ADD_TYPE, sum)
DEFINE_REDUCTION(ELEMENT_ARGMAX_TYPE, argmax)
DEFINE_REDUCTION(ELEMENT_MAX_TYPE, max)
DEFINE_REDUCTION(ELEMENT_MIN_TYPE, min)
DEFINE_REDUCTION(ELEMENT_ARGMIN_TYPE, argmin)

#undef DEFINE_REDUCTION

namespace detail
{

  expression_tree matmatprod(array_base const & A, array_base const & B)
  {
    shape_t shape{A.shape()[0], B.shape()[1]};
    return expression_tree(A, B, op_element(MATRIX_PRODUCT_TYPE_FAMILY, MATRIX_PRODUCT_NN_TYPE), A.context(), A.dtype(), shape);
  }

  expression_tree matmatprod(expression_tree const & A, array_base const & B)
  {
    operation_type type = MATRIX_PRODUCT_NN_TYPE;
    shape_t shape{A.shape()[0], B.shape()[1]};

    expression_tree::node & A_root = const_cast<expression_tree::node &>(A.tree()[A.root()]);
    bool A_trans = A_root.op.type==TRANS_TYPE;
    if(A_trans){
      type = MATRIX_PRODUCT_TN_TYPE;
    }

    expression_tree res(A, B, op_element(MATRIX_PRODUCT_TYPE_FAMILY, type), A.context(), A.dtype(), shape);
    expression_tree::node & res_root = const_cast<expression_tree::node &>(res.tree()[res.root()]);
    if(A_trans) res_root.lhs = A_root.lhs;
    return res;
  }

  expression_tree matmatprod(array_base const & A, expression_tree const & B)
  {
    operation_type type = MATRIX_PRODUCT_NN_TYPE;
    shape_t shape{A.shape()[0], B.shape()[1]};

    expression_tree::node & B_root = const_cast<expression_tree::node &>(B.tree()[B.root()]);
    bool B_trans = B_root.op.type==TRANS_TYPE;
    if(B_trans){
      type = MATRIX_PRODUCT_NT_TYPE;
    }

    expression_tree res(A, B, op_element(MATRIX_PRODUCT_TYPE_FAMILY, type), A.context(), A.dtype(), shape);
    expression_tree::node & res_root = const_cast<expression_tree::node &>(res.tree()[res.root()]);
    if(B_trans) res_root.rhs = B_root.lhs;
    return res;
  }

  expression_tree matmatprod(expression_tree const & A, expression_tree const & B)
  {
    operation_type type = MATRIX_PRODUCT_NN_TYPE;
    expression_tree::node & A_root = const_cast<expression_tree::node &>(A.tree()[A.root()]);
    expression_tree::node & B_root = const_cast<expression_tree::node &>(B.tree()[B.root()]);
    shape_t shape{A.shape()[0], B.shape()[1]};

    bool A_trans = A_root.op.type==TRANS_TYPE;
    bool B_trans = B_root.op.type==TRANS_TYPE;

    if(A_trans && B_trans)  type = MATRIX_PRODUCT_TT_TYPE;
    else if(A_trans && !B_trans) type = MATRIX_PRODUCT_TN_TYPE;
    else if(!A_trans && B_trans) type = MATRIX_PRODUCT_NT_TYPE;
    else type = MATRIX_PRODUCT_NN_TYPE;

    expression_tree res(A, B, op_element(MATRIX_PRODUCT_TYPE_FAMILY, type), A.context(), A.dtype(), shape);
    expression_tree::node & res_root = const_cast<expression_tree::node &>(res.tree()[res.root()]);
    if(A_trans) res_root.lhs = A_root.lhs;
    if(B_trans) res_root.rhs = B_root.lhs;
    return res;
  }

  template<class T>
  expression_tree matvecprod(array_base const & A, T const & x)
  {
    int_t M = A.shape()[0];
    int_t N = A.shape()[1];
    return sum(A*repmat(reshape(x, {1, N}), M, 1), 1);
  }

  template<class T>
  expression_tree matvecprod(expression_tree const & A, T const & x)
  {
    int_t M = A.shape()[0];
    int_t N = A.shape()[1];
    expression_tree::node & A_root = const_cast<expression_tree::node &>(A.tree()[A.root()]);
    bool A_trans = A_root.op.type==TRANS_TYPE;
    while(A_root.lhs.subtype==COMPOSITE_OPERATOR_TYPE){
        A_root = A.tree()[A_root.lhs.node_index];
        A_trans ^= A_root.op.type==TRANS_TYPE;
    }
    if(A_trans)
    {
      expression_tree tmp(A, repmat(x, 1, M), op_element(BINARY_TYPE_FAMILY, ELEMENT_PROD_TYPE), A.context(), A.dtype(), {N, M});
      //Remove trans
      tmp.tree()[tmp.root()].lhs = A.tree()[A.root()].lhs;
      return sum(tmp, 0);
    }
    else
      return sum(A*repmat(reshape(x, {1, N}), M, 1), 1);

  }

}

//Swap
ISAACAPI void swap(view x, view y)
{
  //Seems like some compilers will generate incorrect code without the 1*...
  execute(fuse(assign(y,1*x), assign(x,1*y)));
}

//Reshape
expression_tree reshape(array_base const & x, shape_t const & shape)
{  return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, RESHAPE_TYPE), x.context(), x.dtype(), shape); }

expression_tree reshape(expression_tree const & x, shape_t const & shape)
{  return expression_tree(x, invalid_node(), op_element(UNARY_TYPE_FAMILY, RESHAPE_TYPE), x.context(), x.dtype(), shape); }

expression_tree ravel(array_base const & x)
{ return reshape(x, {x.shape().prod()}); }

#define DEFINE_DOT(LTYPE, RTYPE) \
expression_tree dot(LTYPE const & x, RTYPE const & y)\
{\
  numeric_type dtype = x.dtype();\
  driver::Context const & context = x.context();\
  if(x.shape().max()==1 || y.shape().max()==1)\
    return x*y;\
  if(x.dim()==2 && x.shape()[1]==0)\
    return zeros(x.shape()[0], y.shape()[1], dtype, context);\
  if(x.shape()[0]==0 || (y.dim()==2 && y.shape()[1]==0))\
    return expression_tree(invalid_node(), invalid_node(), op_element(UNARY_TYPE_FAMILY, INVALID_TYPE), context, dtype, {0});\
  if(x.dim()==1 && y.dim()==1)\
    return sum(x*y);\
  if(x.dim()==2 && x.shape()[0]==1 && y.dim()==1){\
    if(y.shape()[0]==1)\
        return reshape(x*y, {x.shape().max()});\
    else\
        return sum(x*y);\
  }\
  if(x.dim()==2 && y.dim()==1){\
    if(y.shape()[0]==1)\
        return reshape(x*y, {x.shape().max()});\
    else\
        return detail::matvecprod(x, y);\
  }\
  if(x.dim()==1 && y.dim()==2){\
    if(x.shape()[0]==1)\
        return reshape(x*y, {y.shape().max()});\
    else\
        return trans(detail::matvecprod(trans(y), trans(x)));\
  }\
  if(x.shape()[0]==1 && y.shape()[1]==1)\
    return sum(x*trans(y));\
  if(x.shape()[0]==1 && y.shape()[1]==2)\
    return trans(detail::matvecprod(trans(y), trans(x)));\
  if(x.shape()[1]==1 && y.shape()[0]==1)\
    return x*y;\
  else /*if(x.dim()==2 && y.dim()==2)*/\
    return detail::matmatprod(x, y);\
}

DEFINE_DOT(array_base, array_base)
DEFINE_DOT(expression_tree, array_base)
DEFINE_DOT(array_base, expression_tree)
DEFINE_DOT(expression_tree, expression_tree)

#undef DEFINE_DOT


#define DEFINE_NORM(TYPE)\
expression_tree norm(TYPE const & x, unsigned int order)\
{\
  assert(order > 0 && order < 3);\
  switch(order)\
  {\
    case 1: return sum(abs(x));\
    default: return sqrt(sum(pow(x,2)));\
  }\
}

DEFINE_NORM(array_base)
DEFINE_NORM(expression_tree)

#undef DEFINE_NORM

/*--- Fusion ----*/
expression_tree fuse(expression_tree const & x, expression_tree const & y)
{
  assert(x.context()==y.context());
  return expression_tree(x, y, op_element(BINARY_TYPE_FAMILY, OPERATOR_FUSE), x.context(), x.dtype(), x.shape());
}

/*--- For loops ---*/
ISAACAPI expression_tree sfor(expression_tree const & start, expression_tree const & end, expression_tree const & inc, expression_tree const & x)
{
  return expression_tree(x, make_tuple(x.context(), start, end, inc), op_element(UNARY_TYPE_FAMILY, SFOR_TYPE), x.context(), x.dtype(), x.shape());
}


/*--- Copy ----*/
//---------------------------------------

//void*
void copy(void const * data, array_base& x, driver::CommandQueue & queue, bool blocking)
{
  unsigned int dtypesize = size_of(x.dtype());
  if(x.start()==0 && x.shape()[0]*x.stride().prod()==x.shape().prod())
  {
    queue.write(x.data(), blocking, 0, x.shape().prod()*dtypesize, data);
  }
  else
  {
    array tmp(x.dtype(), x.shape(), x.context());
    queue.write(tmp.data(), blocking, 0, tmp.shape().prod()*dtypesize, data);
    x = tmp;
  }
}

void copy(array_base const & x, void* data, driver::CommandQueue & queue, bool blocking)
{
  unsigned int dtypesize = size_of(x.dtype());
  if(x.start()==0 && x.stride().prod()==x.shape().prod())
  {
    queue.read(x.data(), blocking, 0, x.shape().prod()*dtypesize, data);
  }
  else
  {
    array tmp(x.dtype(), x.shape(), x.context());
    tmp = x;
    queue.read(tmp.data(), blocking, 0, tmp.shape().prod()*dtypesize, data);
  }
}

void copy(void const *data, array_base &x, bool blocking)
{
  copy(data, x, driver::backend::queues::get(x.context(), 0), blocking);
}

void copy(array_base const & x, void* data, bool blocking)
{
    copy(x, data, driver::backend::queues::get(x.context(), 0), blocking);
}

//std::vector<>
template<class T>
void copy(std::vector<T> const & cx, array_base & x, driver::CommandQueue & queue, bool blocking)
{
  assert((int_t)cx.size()==x.shape().prod());
  copy((void const*)cx.data(), x, queue, blocking);
}

template<class T>
void copy(array_base const & x, std::vector<T> & cx, driver::CommandQueue & queue, bool blocking)
{
  assert((int_t)cx.size()==x.shape().prod());
  copy(x, (void*)cx.data(), queue, blocking);
}

template<class T>
void copy(std::vector<T> const & cx, array_base & x, bool blocking)
{
    copy(cx, x, driver::backend::queues::get(x.context(), 0), blocking);
}

template<class T>
void copy(array_base const & x, std::vector<T> & cx, bool blocking)
{
    copy(x, cx, driver::backend::queues::get(x.context(), 0), blocking);
}

#define INSTANTIATE(T) \
  template void ISAACAPI  copy<T>(std::vector<T> const &, array_base &, driver::CommandQueue&, bool);\
  template void ISAACAPI  copy<T>(array_base const &, std::vector<T> &, driver::CommandQueue&, bool);\
  template void ISAACAPI  copy<T>(std::vector<T> const &, array_base &, bool);\
  template void ISAACAPI  copy<T>(array_base const &, std::vector<T> &, bool)

INSTANTIATE(char);
INSTANTIATE(unsigned char);
INSTANTIATE(short);
INSTANTIATE(unsigned short);
INSTANTIATE(int);
INSTANTIATE(unsigned int);
INSTANTIATE(long);
INSTANTIATE(unsigned long);
INSTANTIATE(long long);
INSTANTIATE(unsigned long long);
INSTANTIATE(float);
INSTANTIATE(double);

#undef INSTANTIATE

/*--- Stream operators----*/
//---------------------------------------

std::ostream& operator<<(std::ostream & os, array_base const & a)
{
  int_t WINDOW = 3;
  shape_t shape = a.shape();
  numeric_type dtype = a.dtype();

  //Copy to Host RAM
  void* tmp = new char[shape.prod()*size_of(dtype)];
  copy(a, (void*)tmp);

  //Strides of the CPU buffer
  std::vector<int_t> strides(shape.size());
  strides[0] = 1;
  for(size_t i = 1 ; i < shape.size() ; ++i)
    strides[i] = strides[i-1]*shape[i-1];

  //Fortran ordering
  for(size_t i = 1 ; i < shape.size(); ++i){
    std::swap(shape[i], shape[i-1]);
    std::swap(strides[i], strides[i-1]);
  }

  //Where to break lines
  std::vector<int_t> linebreaks(shape.size());
  int_t num_displayed = 1;
  for(size_t i = 0 ; i < shape.size() ; ++i)
  {
    linebreaks[i] = num_displayed;
    num_displayed *= std::min(shape[i], 2*WINDOW);
  }

  os << "[" ;
  for(int_t i = 0 ; i < num_displayed ; ++i)
  {

    //Open brackets
    for(size_t s = 1 ; s < shape.size() ; ++s){
        if(i % linebreaks[s] == 0)
            os << "[";
    }

    //Print element
    int_t current = i;
    int_t idx = 0;
    for(int_t s = shape.size() - 1 ; s >= 0 ; --s){
      int_t off = current/linebreaks[s];
      int_t data_off = (shape[s]>2*WINDOW && off+1 > WINDOW)?shape[s] - (2*WINDOW - off):off;
      idx += data_off*strides[s];
      current = current - off*linebreaks[s];
    }
#define ISAAC_PRINT_ELEMENT(ADTYPE, CTYPE) case ADTYPE: os << reinterpret_cast<CTYPE*>(tmp)[idx]; break;
    switch(dtype)
    {
        ISAAC_PRINT_ELEMENT(CHAR_TYPE, char)
        ISAAC_PRINT_ELEMENT(UCHAR_TYPE, unsigned char)
        ISAAC_PRINT_ELEMENT(SHORT_TYPE, short)
        ISAAC_PRINT_ELEMENT(USHORT_TYPE, unsigned short)
        ISAAC_PRINT_ELEMENT(INT_TYPE, int)
        ISAAC_PRINT_ELEMENT(UINT_TYPE, unsigned int)
        ISAAC_PRINT_ELEMENT(LONG_TYPE, long)
        ISAAC_PRINT_ELEMENT(ULONG_TYPE, unsigned long)
        ISAAC_PRINT_ELEMENT(FLOAT_TYPE, float)
        ISAAC_PRINT_ELEMENT(DOUBLE_TYPE, double)
        default: throw unknown_datatype(dtype);
    }
#undef ISAAC_PRINT_ELEMENT

    //Comma
    int_t innermost = (i+1) % (shape.size()==1?num_displayed:linebreaks.back());
    if(shape.front() > 2*WINDOW && innermost == WINDOW)
        os << ",...";
    if(innermost > 0)
        os << ",";

    //Closes brackets + linebreak
    for(size_t s = 1 ; s < shape.size() ; ++s)
    {
        if((i+1) % linebreaks[s] == 0){
            os << "]" << ((i==num_displayed-1)?"":"\n");
            if(shape[s] > 2*WINDOW && (i+1) / linebreaks[s] == WINDOW)
                os << "...," << std::endl;
        }
    }
  }
  os << "]";
  return os;
}

ISAACAPI std::ostream& operator<<(std::ostream & oss, expression_tree const & expression)
{
    return oss << array(expression);
}

}
