/*
 * DoocsBackend.h
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#ifndef MTCA4U_DOOCS_BACKEND_H
#define MTCA4U_DOOCS_BACKEND_H

#include <mutex>
#include <future>

#include <ChimeraTK/DeviceBackendImpl.h>
#include <ChimeraTK/VersionNumber.h>

namespace ChimeraTK {

  class DoocsBackendRegisterAccessorBase;

  /** Backend to access DOOCS control system servers.
   *
   *  The sdm URI should look like this:
   *  sdm://./doocs=FACILITY,DEVICE
   *
   *  FACILITY and DEVICE are the first two components of the DOOCS addresses
   * targeted by this device. The full addess is completed by adding the location
   * and property name from the register path names. Thus the register path names
   *  must be of the form "LOCATION/PROPERTY".
   *
   *  If AccessMode::wait_for_new_data is specified when obtaining accessors,
   * ZeroMQ is used to subscribe to the variable and blocking read() will wait
   * until new data has arrived via the subscribtion. If the flag is not
   *  specified, data will be retrieved through standard RPC calls. Note that in
   * either case a first read transfer is performed upon creation of the accessor
   * to make sure the property exists and the server is reachable. Due to
   * limitations in the DOOCS API it is not possible to test whether a property
   * has been published via ZeroMQ or not, so specifying
   * AccessMode::wait_for_new_data for non-ZeroMQ properties will make read()
   * waiting forever.
   */
  class DoocsBackend : public DeviceBackendImpl {
   public:
    ~DoocsBackend() override;

    DoocsBackend(const std::string& serverAddress, const std::string& cacheFile = {}, const std::string& updateCache = {});

    const RegisterCatalogue& getRegisterCatalogue() const override;

    void open() override;

    void close() override;

    bool isFunctional() const override;

    std::string readDeviceInfo() override { return std::string("DOOCS server address: ") + _serverAddress; }

    void setException() override;

    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);

    template<typename UserType>
    boost::shared_ptr<NDRegisterAccessor<UserType>> getRegisterAccessor_impl(
        const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);
    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER(DoocsBackend, getRegisterAccessor_impl, 4);

    /** DOOCS address component for the server (FACILITY/DEVICE) */
    std::string _serverAddress;

    /** We need to make the catalogue mutable, since we fill it within getRegisterCatalogue() */
    mutable std::unique_ptr<RegisterCatalogue> _catalogue_mutable{};

    /** Class to register the backend type with the factory. */
    class BackendRegisterer {
     public:
      BackendRegisterer();
    };
    static BackendRegisterer backendRegisterer;

    /** Utility function to check if a property is published by ZMQ. */
    static bool checkZmqAvailability(const std::string& fullLocationPath, const std::string& propertyName);

    template<typename UserType>
    friend class DoocsBackendRegisterAccessor;

    /** Called by accessors to inform about addess causing a runtime_error. Does not switch backend into exception
     *  state, this is done separately by calling setException(). */
    void informRuntimeError(const std::string& address);

    void activateAsyncRead() noexcept override;

    std::atomic<bool> _asyncReadActivated{false};

    VersionNumber getStartVersion() {
      std::lock_guard<std::mutex> lk(_mxRecovery);
      return _startVersion;
    }

    static bool isCommunicationError(int doocs_error);

   private:
    std::string _cacheFile;
    mutable std::future<std::unique_ptr<RegisterCatalogue>> _catalogueFuture;
    std::promise<void> _cancelFlag{};

    bool cacheFileExists();
    bool isCachingEnabled() const;

    /// Mutex for accessing _isFunctional, lastFailedAddress and _startVersion;
    mutable std::mutex _mxRecovery;

    bool _isFunctional{false};

    /// VersionNumber generated in open() to make sure we do not violate TransferElement spec B.9.3.3.1/B.9.4.1
    VersionNumber _startVersion{nullptr};

    /// contains DOOCS address which triggered runtime_error, when _isFunctional == false and _opend == true
    std::string lastFailedAddress;
  };

} // namespace ChimeraTK

#endif /* MTCA4U_DOOCS_BACKEND_H */
