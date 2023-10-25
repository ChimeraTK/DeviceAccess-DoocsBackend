#ifndef CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#define CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H
#include "DoocsBackend.h"
#include "EventIdMapper.h"
#include "RegisterInfo.h"
#include "ZMQSubscriptionManager.h"
#include <eq_client.h>
#include <eq_errors.h>
#include <eq_fct.h>
#include <type_traits>

#include <ChimeraTK/AccessMode.h>
#include <ChimeraTK/Exception.h>
#include <ChimeraTK/FixedPointConverter.h>
#include <ChimeraTK/MappedImage.h>
#include <ChimeraTK/NDRegisterAccessor.h>
#include <ChimeraTK/RegisterPath.h>

#include <mutex>
#include <queue>

namespace ChimeraTK {

  /** This is the untemplated base class which unifies all data members not depending on the UserType. */
  class DoocsBackendRegisterAccessorBase {
   public:
    /// register path
    std::string _path;

    /// DOOCS address structure
    EqAdr ea;

    /// DOOCS rpc call object
    EqCall eq;

    /// DOOCS data structures
    EqData src, dst;

    /// flag if the DOOCS data type is an array or not
    bool isArray{false};

    /// number of elements
    size_t nElements{0};

    /// element offset specified by the user
    size_t elementOffset{0};

    /// flag if the accessor should affect only a part of the property (in case of an array)
    bool isPartial{false};

    /// flag if a ZeroMQ subscribtion is used for reading data (c.f. AccessMode::wait_for_new_data)
    bool useZMQ{false};

    /// flag whether it should receive updates from the ZeroMQ subscription. Is used by the ZMQSubscriptionManager and
    /// access requires a lock on the corresponding listeners_mutex.
    bool isActiveZMQ{false};

    /// future_queue used to notify the TransferFuture about completed transfers
    cppext::future_queue<EqData> notifications;

    /// Flag whether shutdown() has been called or not
    bool shutdownCalled{false};

    /// Pointer to the backend
    boost::shared_ptr<DoocsBackend> _backend;

   protected:
    /// first valid eventId
    doocs::EventId _lastEventId;
  };

  /********************************************************************************************************************/

  template<typename UserType>
  class DoocsBackendRegisterAccessor : public DoocsBackendRegisterAccessorBase, public NDRegisterAccessor<UserType> {
   public:
    ~DoocsBackendRegisterAccessor() override;

    /**
     * All implementations must call this function in their destructor. Also,
     * implementations must call it in their constructors before throwing an
     * exception.
     */
    void shutdown() {
      if(useZMQ) {
        DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().unsubscribe(_path, this);
      }
      shutdownCalled = true;
    }

    void doReadTransferSynchronously() override;

    bool doWriteTransfer(VersionNumber) override {
      write_internal();
      return false;
    }

    void doPreRead(TransferType) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Read operation not allowed while device is closed.");
      if(!isReadable()) throw ChimeraTK::logic_error("Try to read from write-only register \"" + _path + "\".");
    }

    void doPreWrite(TransferType, VersionNumber) override {
      if(!_backend->isOpen()) throw ChimeraTK::logic_error("Write operation not allowed while device is closed.");
      if(!isWriteable()) throw ChimeraTK::logic_error("Try to write read-only register \"" + _path + "\".");
    }

    void doPostRead(TransferType, bool hasNewData) override {
      if(!hasNewData) return;

      // Note: the original idea was to extract the time stamp from the received data. This idea has been dropped since
      // the time stamp attached to the data seems to be unreliably, at least for the x2timer macro pulse number. If the
      // unreliable time stamp is attached to the trigger, all data will get this time stamp. This leads to error
      // messages of the DOOCS history archiver, which rejects data due to wrong time stamps. Hence we better generate
      // our own time stamp here.

      // See spec. B.1.3.4.2
      TransferElement::setDataValidity(dst.error() == 0 ? DataValidity::ok : DataValidity::faulty);

      // If the eventid is valid (!= 0) but older than the last one, we have backward running eventids but VersionNumber must not run backwards.
      // Keep VersionNumber unchanged and return.
      // See spec B.1.3.2.
      // (Note: after a re-connection to a slow variable the version number might be the same)
      if(dst.get_event_id() != doocs::EventId(0) && dst.get_event_id() < _lastEventId) {
        return;
      }

      if(dst.get_event_id() == doocs::EventId()) {
        // See spec. B.1.3.4.1
        TransferElement::_versionNumber = {};
        _lastEventId = dst.get_event_id();
        return;
      }

      // Only check for a new version number if the event ids no not match up.
      // Otherwise it might be possible that the EventIDMapper allready removed the old event id from
      // its cache and will generate a new versionnumber for the same event id.
      // This prevents not updating slow- if ever changing variables that are polled periodically
      //
      // During startup, we can receive multiple receives with event_id == 0. The first check ensures that
      // we do not hand out the VersionNumber{nullptr} then
      //if(_lastEventId == doocs::EventId() || _lastEventId != dst.get_event_id()) {
      // Get VersionNumber from the EventIdMapper. See spec B.1.3.3.
      auto newVersionNumber = EventIdMapper::getInstance().getVersionForEventId(dst.get_event_id());

      // Minimum version is _backend->_startVersion. See spec. B.1.3.3.1.
      auto startVersion = _backend->getStartVersion();
      if(newVersionNumber < startVersion) {
        newVersionNumber = startVersion;
      }

      // Version still must not go backwards. See spec B.1.3.3.2.
      // If the sender is sending a valid eventid, afterwards sends eventid 0, and after that sends
      // same eventid again, then the last sent version in this transfer element has been increased
      // by eventid 0 and now newVersionNumber which is coming from the EventIDMapper is older.
      if(newVersionNumber < TransferElement::_versionNumber) {
        return;
      }

      // See spec. B.1.3.4.1
      TransferElement::_versionNumber = newVersionNumber;
      _lastEventId = dst.get_event_id();
    }

    bool isReadOnly() const override { return isReadable() && not isWriteable(); }

    bool isReadable() const override { return _isReadable; }

    bool isWriteable() const override { return _isWriteable; }

    using TransferElement::_readQueue;

    bool mayReplaceOther(const boost::shared_ptr<TransferElement const>& other) const override {
      auto rhsCasted = boost::dynamic_pointer_cast<const DoocsBackendRegisterAccessor<UserType>>(other);
      if(!rhsCasted) return false;
      if(_path != rhsCasted->_path) return false;
      if(nElements != rhsCasted->nElements) return false;
      if(elementOffset != rhsCasted->elementOffset) return false;
      return true;
    }

    std::vector<boost::shared_ptr<TransferElement>> getHardwareAccessingElements() override {
      return {boost::enable_shared_from_this<TransferElement>::shared_from_this()};
    }

    std::list<boost::shared_ptr<ChimeraTK::TransferElement>> getInternalElements() override { return {}; }

    void replaceTransferElement(boost::shared_ptr<TransferElement> /*newElement*/) override {} // LCOV_EXCL_LINE

    void interrupt() override { this->interrupt_impl(this->notifications); }

   protected:
    DoocsBackendRegisterAccessor(boost::shared_ptr<DoocsBackend> backend, const std::string& path,
        const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister, AccessModeFlags flags);

    /// internal write from EqData src
    void write_internal();

    /**
     *  Perform initialisation (i.e. connect to server etc.).
     *
     *  Note: must *only* throw ChimeraTK::logic_error. Just do not proceed with the initialisation if a runtime_error
     *  is to be thrown - this will then be done in the transfer.
     */
    void initialise(const DoocsBackendRegisterInfo& info);

    bool _isReadable;
    bool _isWriteable;
  };

  /********************************************************************************************************************/
  /********************************************************************************************************************/
  /** Implementations below this point                                                                               **/
  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::initialise(const DoocsBackendRegisterInfo& info) {
    size_t actualLength = 0;
    int typeId = 0;

    // Try to read data (if the device is opened), to obtain type and size of the register. Otherwise take information
    // from catalogue (-> cache)
    int rc = 1;
    if(_backend->isOpen()) {
      EqData tmp;
      rc = eq.get(&ea, &tmp, &dst);
    }
    if(rc) {
      if(rc == eq_errors::ill_property || rc == eq_errors::ill_location || rc == eq_errors::ill_address) {
        // no property by that name
        throw ChimeraTK::logic_error("Property does not exist: " + _path);
      }

      // we cannot reach the server, so try to obtain information from the catalogue
      actualLength = info.getNumberOfElements();
      typeId = info.doocsTypeId;
    }
    else {
      // obtain number of elements from server reply
      actualLength = dst.array_length();
      if(actualLength == 0 && dst.length() == 1) {
        actualLength = 1;
      }
      else {
        if(actualLength == 0) actualLength = dst.length();
      }
      typeId = dst.type();
      if(typeId == DATA_IMAGE) {
        // RegisterAccessor byte array needs to store header and body
        actualLength += sizeof(ChimeraTK::ImgHeader);
      }
    }

    // strings report number of characters, not number of strings..
    if(typeId == DATA_TEXT || typeId == DATA_STRING) {
      actualLength = 1;
    }

    if(actualLength > 1) {
      isArray = true;
    }
    else {
      isArray = false;
    }

    if(nElements == 0) {
      nElements = actualLength;
    }
    if(nElements + elementOffset > actualLength) {
      // for image type, we allow having a byte array larger than required
      if(typeId != DATA_IMAGE) {
        throw ChimeraTK::logic_error("Requested number of words exceeds the length of the DOOCS property!");
      }
    }
    if(nElements == actualLength && elementOffset == 0) {
      isPartial = false;
    }
    else {
      isPartial = true;
    }

    // allocate buffers
    NDRegisterAccessor<UserType>::buffer_2D.resize(1);
    NDRegisterAccessor<UserType>::buffer_2D[0].resize(nElements);

    // set proper type information in the source EqData
    src.set_type(typeId);
    if(typeId == DATA_IMAGE) {
      // DOOCS data structure has its own image header format and length() is only used for body length
      src.length(actualLength - sizeof(ChimeraTK::ImgHeader));
    }
    else if(typeId != DATA_IIII) {
      src.length(actualLength);
    }

    // use ZeroMQ with AccessMode::wait_for_new_data
    if(useZMQ) {
      // subscribe via subscription manager
      DoocsBackendNamespace::ZMQSubscriptionManager::getInstance().subscribe(_path, this);
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::DoocsBackendRegisterAccessor(boost::shared_ptr<DoocsBackend> backend,
      const std::string& path, const std::string& registerPathName, size_t numberOfWords, size_t wordOffsetInRegister,
      AccessModeFlags flags)
  : NDRegisterAccessor<UserType>(path, flags), _isReadable(true), _isWriteable(true) {
    try {
      _backend = backend;
      _path = path;
      elementOffset = wordOffsetInRegister;
      nElements = numberOfWords;
      useZMQ = false;

      // check for unknown access mode flags
      flags.checkForUnknownFlags({AccessMode::wait_for_new_data});

      // set address
      ea.adr(path);

      // obtain catalogue entry
      auto info = backend->getBackendRegisterCatalogue().getBackendRegister(registerPathName);

      // use zero mq subscriptiopn?
      if(flags.has(AccessMode::wait_for_new_data)) {
        if(!info.getSupportedAccessModes().has(AccessMode::wait_for_new_data)) {
          throw ChimeraTK::logic_error("invalid access mode for this register");
        }
        useZMQ = true;

        // Create notification queue.
        notifications = cppext::future_queue<EqData>(3);
        _readQueue = notifications.then<void>([this](EqData& data) { this->dst = data; }, std::launch::deferred);
      }

      initialise(info);
    }
    catch(...) {
      this->shutdown();
      throw;
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  DoocsBackendRegisterAccessor<UserType>::~DoocsBackendRegisterAccessor() {
    assert(shutdownCalled);
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::doReadTransferSynchronously() {
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }

    assert(!useZMQ);

    boost::this_thread::interruption_point();

    // read data
    EqData tmp;
    int rc = eq.get(&ea, &tmp, &dst);

    // check error
    if(rc && DoocsBackend::isCommunicationError(dst.error())) {
      _backend->informRuntimeError(_path);
      throw ChimeraTK::runtime_error(std::string("Cannot read from DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

  template<typename UserType>
  void DoocsBackendRegisterAccessor<UserType>::write_internal() {
    if(!_backend->isFunctional()) {
      throw ChimeraTK::runtime_error(std::string("Exception reported by another accessor."));
    }

    // write data
    int rc = eq.set(&ea, &src, &dst);
    // check error
    if(rc && (DoocsBackend::isCommunicationError(dst.error()) || (dst.error() == eq_errors::read_only))) {
      _backend->informRuntimeError(_path);
      if(dst.error() == eq_errors::read_only) {
        this->_isWriteable = false;
      }
      throw ChimeraTK::runtime_error(std::string("Cannot write to DOOCS property: ") + dst.get_string());
    }
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK

#endif /* CHIMERATK_DOOCS_BACKEND_REGISTER_ACCESSOR_H */
