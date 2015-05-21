/**
 * @file    RegularImplicitSchurFactor.h
 * @brief   A new type of linear factor (GaussianFactor), which is subclass of GaussianFactor
 * @author  Frank Dellaert
 * @author  Luca Carlone
 */

#pragma once

#include <gtsam/linear/JacobianFactor.h>
#include <gtsam/linear/VectorValues.h>
#include <boost/foreach.hpp>
#include <iosfwd>

namespace gtsam {

/**
 * RegularImplicitSchurFactor
 */
template<size_t D> //
class RegularImplicitSchurFactor: public GaussianFactor {

public:
  typedef RegularImplicitSchurFactor This; ///< Typedef to this class
  typedef boost::shared_ptr<This> shared_ptr; ///< shared_ptr to this class

protected:

  typedef Eigen::Matrix<double, 2, D> Matrix2D; ///< type of an F block
  typedef Eigen::Matrix<double, D, D> MatrixDD; ///< camera hessian
  typedef std::pair<Key, Matrix2D> KeyMatrix2D; ///< named F block

  std::vector<KeyMatrix2D> Fblocks_; ///< All 2*D F blocks (one for each camera)
  Matrix3 PointCovariance_; ///< the 3*3 matrix P = inv(E'E) (2*2 if degenerate)
  Matrix E_; ///< The 2m*3 E Jacobian with respect to the point
  Vector b_; ///< 2m-dimensional RHS vector

public:

  /// Constructor
  RegularImplicitSchurFactor() {
  }

  /// Construct from blcoks of F, E, inv(E'*E), and RHS vector b
  RegularImplicitSchurFactor(const std::vector<KeyMatrix2D>& Fblocks, const Matrix& E,
      const Matrix3& P, const Vector& b) :
      Fblocks_(Fblocks), PointCovariance_(P), E_(E), b_(b) {
    initKeys();
  }

  /// initialize keys from Fblocks
  void initKeys() {
    keys_.reserve(Fblocks_.size());
    BOOST_FOREACH(const KeyMatrix2D& it, Fblocks_)
      keys_.push_back(it.first);
  }

  /// Destructor
  virtual ~RegularImplicitSchurFactor() {
  }

  // Write access, only use for construction!

  inline std::vector<KeyMatrix2D>& Fblocks() {
    return Fblocks_;
  }

  inline Matrix3& PointCovariance() {
    return PointCovariance_;
  }

  inline Matrix& E() {
    return E_;
  }

  inline Vector& b() {
    return b_;
  }

  /// Get matrix P
  inline const Matrix3& getPointCovariance() const {
    return PointCovariance_;
  }

  /// print
  void print(const std::string& s = "",
      const KeyFormatter& keyFormatter = DefaultKeyFormatter) const {
    std::cout << " RegularImplicitSchurFactor " << std::endl;
    Factor::print(s);
    for (size_t pos = 0; pos < size(); ++pos) {
      std::cout << "Fblock:\n" << Fblocks_[pos].second << std::endl;
    }
    std::cout << "PointCovariance:\n" << PointCovariance_ << std::endl;
    std::cout << "E:\n" << E_ << std::endl;
    std::cout << "b:\n" << b_.transpose() << std::endl;
  }

  /// equals
  bool equals(const GaussianFactor& lf, double tol) const {
    const This* f = dynamic_cast<const This*>(&lf);
    if (!f)
      return false;
    for (size_t pos = 0; pos < size(); ++pos) {
      if (keys_[pos] != f->keys_[pos]) return false;
      if (Fblocks_[pos].first != f->Fblocks_[pos].first) return false;
      if (!equal_with_abs_tol(Fblocks_[pos].second,f->Fblocks_[pos].second,tol)) return false;
    }
    return equal_with_abs_tol(PointCovariance_, f->PointCovariance_, tol)
        && equal_with_abs_tol(E_, f->E_, tol)
        && equal_with_abs_tol(b_, f->b_, tol);
  }

  /// Degrees of freedom of camera
  virtual DenseIndex getDim(const_iterator variable) const {
    return D;
  }

  virtual Matrix augmentedJacobian() const {
    throw std::runtime_error(
        "RegularImplicitSchurFactor::augmentedJacobian non implemented");
    return Matrix();
  }
  virtual std::pair<Matrix, Vector> jacobian() const {
    throw std::runtime_error("RegularImplicitSchurFactor::jacobian non implemented");
    return std::make_pair(Matrix(), Vector());
  }
  virtual Matrix augmentedInformation() const {
    throw std::runtime_error(
        "RegularImplicitSchurFactor::augmentedInformation non implemented");
    return Matrix();
  }
  virtual Matrix information() const {
    throw std::runtime_error(
        "RegularImplicitSchurFactor::information non implemented");
    return Matrix();
  }

  /// Return the diagonal of the Hessian for this factor
  virtual VectorValues hessianDiagonal() const {
    // diag(Hessian) = diag(F' * (I - E * PointCov * E') * F);
    VectorValues d;

    for (size_t pos = 0; pos < size(); ++pos) { // for each camera
      Key j = keys_[pos];

      // Calculate Fj'*Ej for the current camera (observing a single point)
      // D x 3 = (D x 2) * (2 x 3)
      const Matrix2D& Fj = Fblocks_[pos].second;
      Eigen::Matrix<double, D, 3>  FtE = Fj.transpose()
          * E_.block<2, 3>(2 * pos, 0);

      Eigen::Matrix<double, D, 1> dj;
      for (size_t k = 0; k < D; ++k) { // for each diagonal element of the camera hessian
        // Vector column_k_Fj = Fj.col(k);
        dj(k) = Fj.col(k).squaredNorm(); // dot(column_k_Fj, column_k_Fj);
        // Vector column_k_FtE = FtE.row(k);
        // (1 x 1) = (1 x 3) * (3 * 3) * (3 x 1)
        dj(k) -= FtE.row(k) * PointCovariance_ * FtE.row(k).transpose();
      }
      d.insert(j, dj);
    }
    return d;
  }

  /**
   * @brief add the contribution of this factor to the diagonal of the hessian
   * d(output) = d(input) + deltaHessianFactor
   */
  virtual void hessianDiagonal(double* d) const {
    // diag(Hessian) = diag(F' * (I - E * PointCov * E') * F);
    // Use eigen magic to access raw memory
    typedef Eigen::Matrix<double, D, 1> DVector;
    typedef Eigen::Map<DVector> DMap;

    for (size_t pos = 0; pos < size(); ++pos) { // for each camera in the factor
      Key j = keys_[pos];

      // Calculate Fj'*Ej for the current camera (observing a single point)
      // D x 3 = (D x 2) * (2 x 3)
      const Matrix2D& Fj = Fblocks_[pos].second;
      Eigen::Matrix<double, D, 3> FtE = Fj.transpose()
          * E_.block<2, 3>(2 * pos, 0);

      DVector dj;
      for (size_t k = 0; k < D; ++k) { // for each diagonal element of the camera hessian
        dj(k) = Fj.col(k).squaredNorm();
        // (1 x 1) = (1 x 3) * (3 * 3) * (3 x 1)
        dj(k) -= FtE.row(k) * PointCovariance_ * FtE.row(k).transpose();
      }
      DMap(d + D * j) += dj;
    }
  }

  /// Return the block diagonal of the Hessian for this factor
  virtual std::map<Key, Matrix> hessianBlockDiagonal() const {
    std::map<Key, Matrix> blocks;
    // F'*(I - E*P*E')*F
    for (size_t pos = 0; pos < size(); ++pos) {
      Key j = keys_[pos];
      // F'*F - F'*E*P*E'*F   (9*2)*(2*9) - (9*2)*(2*3)*(3*3)*(3*2)*(2*9)
      const Matrix2D& Fj = Fblocks_[pos].second;
      //      Eigen::Matrix<double, D, 3> FtE = Fj.transpose()
      //          * E_.block<2, 3>(2 * pos, 0);
      //      blocks[j] = Fj.transpose() * Fj
      //          - FtE * PointCovariance_ * FtE.transpose();

      const Matrix23& Ej = E_.block<2, 3>(2 * pos, 0);
      blocks[j] = Fj.transpose() * (Fj - Ej * PointCovariance_ * Ej.transpose() * Fj);

      // F'*(I - E*P*E')*F, TODO: this should work, but it does not :-(
      //      static const Eigen::Matrix<double, 2, 2> I2 = eye(2);
      //      Matrix2 Q = //
      //          I2 - E_.block<2, 3>(2 * pos, 0) * PointCovariance_ * E_.block<2, 3>(2 * pos, 0).transpose();
      //      blocks[j] = Fj.transpose() * Q * Fj;
    }
    return blocks;
  }

  virtual GaussianFactor::shared_ptr clone() const {
    return boost::make_shared<RegularImplicitSchurFactor<D> >(Fblocks_,
        PointCovariance_, E_, b_);
    throw std::runtime_error("RegularImplicitSchurFactor::clone non implemented");
  }
  virtual bool empty() const {
    return false;
  }

  virtual GaussianFactor::shared_ptr negate() const {
    return boost::make_shared<RegularImplicitSchurFactor<D> >(Fblocks_,
        PointCovariance_, E_, b_);
    throw std::runtime_error("RegularImplicitSchurFactor::negate non implemented");
  }

  // Raw Vector version of y += F'*alpha*(I - E*P*E')*F*x, for testing
  static
  void multiplyHessianAdd(const Matrix& F, const Matrix& E,
      const Matrix& PointCovariance, double alpha, const Vector& x, Vector& y) {
    Vector e1 = F * x;
    Vector d1 = E.transpose() * e1;
    Vector d2 = PointCovariance * d1;
    Vector e2 = E * d2;
    Vector e3 = alpha * (e1 - e2);
    y += F.transpose() * e3;
  }

  typedef std::vector<Vector2> Error2s;

  /**
   * @brief Calculate corrected error Q*(e-2*b) = (I - E*P*E')*(e-2*b)
   */
  void projectError2(const Error2s& e1, Error2s& e2) const {

    // d1 = E.transpose() * (e1-2*b) = (3*2m)*2m
    Vector3 d1;
    d1.setZero();
    for (size_t k = 0; k < size(); k++)
      d1 += E_.block < 2, 3 > (2 * k, 0).transpose() * (e1[k] - 2 * b_.segment < 2 > (k * 2));

    // d2 = E.transpose() * e1 = (3*2m)*2m
    Vector3 d2 = PointCovariance_ * d1;

    // e3 = alpha*(e1 - E*d2) = 1*[2m-(2m*3)*3]
    for (size_t k = 0; k < size(); k++)
      e2[k] = e1[k] - 2 * b_.segment < 2 > (k * 2) - E_.block < 2, 3 > (2 * k, 0) * d2;
  }

  /*
   * This definition matches the linearized error in the Hessian Factor:
   * LinError(x) = x'*H*x - 2*x'*eta + f
   * with:
   * H   = F' * (I-E'*P*E) * F = F' * Q * F
   * eta = F' * (I-E'*P*E) * b = F' * Q * b
   * f = nonlinear error
   * (x'*H*x - 2*x'*eta + f) = x'*F'*Q*F*x - 2*x'*F'*Q *b + f = x'*F'*Q*(F*x - 2*b) + f
   */
  virtual double error(const VectorValues& x) const {

    // resize does not do malloc if correct size
    e1.resize(size());
    e2.resize(size());

    // e1 = F * x - b = (2m*dm)*dm
    for (size_t k = 0; k < size(); ++k)
      e1[k] = Fblocks_[k].second * x.at(keys_[k]);
    projectError2(e1, e2);

    double result = 0;
    for (size_t k = 0; k < size(); ++k)
      result += dot(e1[k], e2[k]);

    double f = b_.squaredNorm();
    return 0.5 * (result + f);
  }

  // needed to be GaussianFactor - (I - E*P*E')*(F*x - b)
  // This is wrong and does not match the definition in Hessian,
  // but it matches the definition of the Jacobian factor (JF)
  double errorJF(const VectorValues& x) const {

    // resize does not do malloc if correct size
    e1.resize(size());
    e2.resize(size());

    // e1 = F * x - b = (2m*dm)*dm
    for (size_t k = 0; k < size(); ++k)
      e1[k] = Fblocks_[k].second * x.at(keys_[k]) - b_.segment < 2 > (k * 2);
    projectError(e1, e2);

    double result = 0;
    for (size_t k = 0; k < size(); ++k)
      result += dot(e2[k], e2[k]);

    // std::cout << "implicitFactor::error result " << result << std::endl;
    return 0.5 * result;
  }
  /**
   * @brief Calculate corrected error Q*e = (I - E*P*E')*e
   */
    void projectError(const Error2s& e1, Error2s& e2) const {

      // d1 = E.transpose() * e1 = (3*2m)*2m
      Vector3 d1;
      d1.setZero();
      for (size_t k = 0; k < size(); k++)
        d1 += E_.block < 2, 3 > (2 * k, 0).transpose() * e1[k];

      // d2 = E.transpose() * e1 = (3*2m)*2m
      Vector3 d2 = PointCovariance_ * d1;

      // e3 = alpha*(e1 - E*d2) = 1*[2m-(2m*3)*3]
      for (size_t k = 0; k < size(); k++)
        e2[k] = e1[k] - E_.block < 2, 3 > (2 * k, 0) * d2;
    }

  /// Scratch space for multiplyHessianAdd
  mutable Error2s e1, e2;

  /**
   * @brief double* Hessian-vector multiply, i.e. y += F'*alpha*(I - E*P*E')*F*x
   * RAW memory access! Assumes keys start at 0 and go to M-1, and x and and y are laid out that way
   */
  void multiplyHessianAdd(double alpha, const double* x, double* y) const {

    // Use eigen magic to access raw memory
    typedef Eigen::Matrix<double, D, 1> DVector;
    typedef Eigen::Map<DVector> DMap;
    typedef Eigen::Map<const DVector> ConstDMap;

    // resize does not do malloc if correct size
    e1.resize(size());
    e2.resize(size());

    // e1 = F * x = (2m*dm)*dm
    size_t k = 0;
    BOOST_FOREACH(const KeyMatrix2D& it, Fblocks_) {
      Key key = it.first;
      e1[k++] = it.second * ConstDMap(x + D * key);
    }

    projectError(e1, e2);

    // y += F.transpose()*e2 = (2d*2m)*2m
    k = 0;
    BOOST_FOREACH(const KeyMatrix2D& it, Fblocks_) {
      Key key = it.first;
      DMap(y + D * key) += it.second.transpose() * alpha * e2[k++];
    }
  }

  void multiplyHessianAdd(double alpha, const double* x, double* y,
      std::vector<size_t> keys) const {
  }
  ;

  /**
   * @brief Hessian-vector multiply, i.e. y += F'*alpha*(I - E*P*E')*F*x
   */
  void multiplyHessianAdd(double alpha, const VectorValues& x,
      VectorValues& y) const {

    // resize does not do malloc if correct size
    e1.resize(size());
    e2.resize(size());

    // e1 = F * x = (2m*dm)*dm
    for (size_t k = 0; k < size(); ++k)
      e1[k] = Fblocks_[k].second * x.at(keys_[k]);

    projectError(e1, e2);

    // y += F.transpose()*e2 = (2d*2m)*2m
    for (size_t k = 0; k < size(); ++k) {
      Key key = keys_[k];
      static const Vector empty;
      std::pair<VectorValues::iterator, bool> it = y.tryInsert(key, empty);
      Vector& yi = it.first->second;
      // Create the value as a zero vector if it does not exist.
      if (it.second)
        yi = Vector::Zero(Fblocks_[k].second.cols());
      yi += Fblocks_[k].second.transpose() * alpha * e2[k];
    }
  }

  /**
   * @brief Dummy version to measure overhead of key access
   */
  void multiplyHessianDummy(double alpha, const VectorValues& x,
      VectorValues& y) const {

    BOOST_FOREACH(const KeyMatrix2D& Fi, Fblocks_) {
      static const Vector empty;
      Key key = Fi.first;
      std::pair<VectorValues::iterator, bool> it = y.tryInsert(key, empty);
      Vector& yi = it.first->second;
      yi = x.at(key);
    }
  }

  /**
   * Calculate gradient, which is -F'Q*b, see paper
   */
  VectorValues gradientAtZero() const {
    // calculate Q*b
    e1.resize(size());
    e2.resize(size());
    for (size_t k = 0; k < size(); k++)
      e1[k] = b_.segment < 2 > (2 * k);
    projectError(e1, e2);

    // g = F.transpose()*e2
    VectorValues g;
    for (size_t k = 0; k < size(); ++k) {
      Key key = keys_[k];
      g.insert(key, -Fblocks_[k].second.transpose() * e2[k]);
    }

    // return it
    return g;
  }

  /**
   * Calculate gradient, which is -F'Q*b, see paper - RAW MEMORY ACCESS
   */
  virtual void gradientAtZero(double* d) const {

    // Use eigen magic to access raw memory
    typedef Eigen::Matrix<double, D, 1> DVector;
    typedef Eigen::Map<DVector> DMap;

    // calculate Q*b
    e1.resize(size());
    e2.resize(size());
    for (size_t k = 0; k < size(); k++)
      e1[k] = b_.segment < 2 > (2 * k);
    projectError(e1, e2);

    for (size_t k = 0; k < size(); ++k) { // for each camera in the factor
      Key j = keys_[k];
      DMap(d + D * j) += -Fblocks_[k].second.transpose() * e2[k];
    }
  }

  /// Gradient wrt a key at any values
  Vector gradient(Key key, const VectorValues& x) const {
    throw std::runtime_error("gradient for RegularImplicitSchurFactor is not implemented yet");
  }


};
// end class RegularImplicitSchurFactor

// traits
template<size_t D> struct traits<RegularImplicitSchurFactor<D> > : public Testable<
    RegularImplicitSchurFactor<D> > {
};

}
