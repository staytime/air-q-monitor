#ifndef FIXPOINT_H_
#define FIXPOINT_H_

template<typename T, size_t F>
class FIXPOINT {
private:
  static const T basenum = (1 << F);
  T num;
public:
  FIXPOINT(): num(0) {}

  FIXPOINT(const int& n) {
    num = n * basenum;
  }

  FIXPOINT(const float& n) {
    num = (T) (n * basenum);
  }

  inline FIXPOINT& operator += (const FIXPOINT& y) {
    num += y.num;
    return *this;
  }

  inline FIXPOINT& operator += (const T& src) {
    num += (src * basenum);
    return *this;
  }

  inline FIXPOINT& operator += (const float& src) {
    num += (T) (src * basenum);
    return *this;
  }

  inline FIXPOINT& operator *= (const T& y) {
    num *= y;
    return *this;
  }

  inline FIXPOINT operator * (const T& y) {
    FIXPOINT x = *this;
    return x *= y;
  }

  inline operator T () const {
    return (num >> F);
  }

  inline operator float () const {
    return to_float();
  }

  inline bool operator == (const FIXPOINT& y) {
    return num == y.num;
  }

  inline bool operator != (const FIXPOINT& y) {
    return !((*this) == y);
  }

  inline bool operator > (const FIXPOINT& y) {
    return num > y.num;
  }

  inline bool operator < (const FIXPOINT& y) {
    return num < y.num;
  }

  inline FIXPOINT& operator = (const T& src) {
    num = (src * basenum);
    return *this;
  }

  inline FIXPOINT& operator = (const float& src) {
    num = (T) (src * basenum);
    return *this;
  }

  inline const uint8_t *const serialize() {
    return (const uint8_t *const) &num;
  }

  inline const int to_int() const {
    return num >> F;
  }

  inline const float to_float() const {
    float temp = num;
    return temp / basenum;
  }

  inline const T& getRaw() const {
    return num;
  }
  

};


typedef FIXPOINT<int16_t, 4> T_FIXPOINT_16F4;
typedef FIXPOINT<int32_t, 4> T_FIXPOINT_32F4;

inline void serializeWrite2Buf(uint8_t *const buf, const size_t &offset, const T_FIXPOINT_16F4& data) {
  auto temp = data.getRaw();
  for(size_t i = 0; i < 2; ++i) {
    buf[offset + i] = (uint8_t) (temp & 0xff);
    temp >>= 8;
  }
}

inline void serializeWrite2Buf(uint8_t *const buf, const size_t &offset, const T_FIXPOINT_32F4& data) {
  auto temp = data.getRaw();
  for(size_t i = 0; i < 4; ++i) {
    buf[offset + i] = (uint8_t) (temp & 0xff);
    temp >>= 8;
  }
}

#endif