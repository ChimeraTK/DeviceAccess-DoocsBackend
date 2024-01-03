/*
 * DoocsBackend.h
 *
 *  Created on: Apr 26, 2016
 *      Author: Martin Hierholzer
 */

#pragma once

#include "RegisterInfo.h"

#include <ChimeraTK/DeviceBackendImpl.h>
#include <ChimeraTK/VersionNumber.h>

#include <future>
#include <mutex>

namespace ChimeraTK {

  class DoocsBackendRegisterAccessorBase;

  /**
   * Backend to access DOOCS control system servers.
   *
   * The CDD should look like this:
   *
   * (doocs:FACILITY/DEVICE)
   *
   * or:
   *
   * (doocs:FACILITY/DEVICE/LOCATION)
   *
   * The given path components are prefixed to the register names, hence if the LOCATION is not specified in the CDD,
   * the register names will be of the form LOCATION/PROPERTY, while otherwise the register names will be just the
   * property names.
   *
   * If the parameter "cacheFile" in the CDD is specified, e.g.:
   *
   * (doocs:FACILITY/DEVICE/LOCATION?cacheFile=myDooceDevice.cache)
   *
   * the register catalogue will be cached in the given file and reused next time. This allows to speed up the filling
   * of the catalogue (which may take a very long time for large number of properties).
   *
   * Please note that the cache file is only updated when the "updateCache" parameter is set to 1, e.g.:
   *
   * (doocs:FACILITY/DEVICE/LOCATION?cacheFile=myDooceDevice.cache&updateCache=1)
   *
   * Otherwise no catalogue updating will be initiated if the cache file is already present.
   *
   * If AccessMode::wait_for_new_data is specified when obtaining accessors, ZeroMQ is used to subscribe to the variable
   * and blocking read() will wait until new data has arrived via the subscribtion. If the flag is not specified, data
   * will be retrieved through standard RPC calls. Note that in either case a first read transfer is performed upon
   * creation of the accessor to make sure the property exists and the server is reachable, and to obtain the initial
   * value.
   */
  class DoocsBackend : public DeviceBackendImpl {
   public:
    ~DoocsBackend() override;

    DoocsBackend(
        const std::string& serverAddress, const std::string& cacheFile = {}, const std::string& updateCache = {});

    RegisterCatalogue getRegisterCatalogue() const override;

    const DoocsBackendRegisterCatalogue& getBackendRegisterCatalogue() const;

    void open() override;

    void close() override;

    std::string readDeviceInfo() override { return std::string("DOOCS server address: ") + _serverAddress; }

    void setExceptionImpl() noexcept override;

    static boost::shared_ptr<DeviceBackend> createInstance(
        std::string address, std::map<std::string, std::string> parameters);

    template<typename UserType>
    boost::shared_ptr<NDRegisterAccessor<UserType>> getRegisterAccessor_impl(
        const RegisterPath& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);
    DEFINE_VIRTUAL_FUNCTION_TEMPLATE_VTABLE_FILLER(DoocsBackend, getRegisterAccessor_impl, 4);

    /** DOOCS address component for the server (FACILITY/DEVICE) */
    std::string _serverAddress;

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
    std::promise<void> _cancelFlag{};
    mutable std::future<DoocsBackendRegisterCatalogue> _catalogueFuture;
    mutable DoocsBackendRegisterCatalogue catalogue;

    bool cacheFileExists();
    bool isCachingEnabled() const;

    /// Mutex for accessing  lastFailedAddress and _startVersion;
    mutable std::mutex _mxRecovery;

    /// VersionNumber generated in open() to make sure we do not violate TransferElement spec B.9.3.3.1/B.9.4.1
    VersionNumber _startVersion{nullptr};

    /// contains DOOCS address which triggered runtime_error, when _hasActiveException == true and _opend == true
    std::string lastFailedAddress;
  };

} // namespace ChimeraTK
