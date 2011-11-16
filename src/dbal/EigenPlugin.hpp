inline bool is_finite() const {
    for (Index j = 0; j < size(); ++j)
        if (!boost::math::isfinite(this->coeff(j)))
            return false;
    return true;
}
