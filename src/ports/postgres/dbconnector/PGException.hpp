class AbstractionLayer::PGException
  : public std::runtime_error {

public:
    explicit 
    PGException()
      : std::runtime_error("The backend raised an exception.") { }
};
