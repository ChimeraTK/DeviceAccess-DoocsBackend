#ifndef CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H
#define CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H

#include <eq_fct.h>

#include <ChimeraTK/Exception.h>

#include <boost/shared_ptr.hpp>

#include <map>
#include <mutex>
#include <pthread.h>
#include <string>
#include <vector>

namespace ChimeraTK {

  class DoocsBackendRegisterAccessorBase;
  class DoocsBackend;

  namespace DoocsBackendNamespace {

    class ZMQSubscriptionManager {
     public:
      static ZMQSubscriptionManager& getInstance() {
        static ZMQSubscriptionManager manager;
        return manager;
      }

      /// Register accessor subscription
      void subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor);

      /// Unregister accessor subscription
      void unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor);

      /// Activate all listeners for the given backend. Should be called from DoocsBackend::activateAsyncRead().
      void activateAllListeners(DoocsBackend* backend);

      /// Deactivate all listeners the given backend. Should be called from DoocsBackend::close().
      void deactivateAllListeners(DoocsBackend* backend);

      /// Deactivate all listeners for the given backend and push exceptions into the queues. Should be called from
      /// DoocsBackend::setException().
      void deactivateAllListenersAndPushException(DoocsBackend* backend);

     private:
      ZMQSubscriptionManager();
      ~ZMQSubscriptionManager();

      // Activate ZeroMQ subscription.
      // Caller need to own subscriptionMap_mutex already.
      void activateSubscription(const std::string& path);

      // Deactivate ZeroMQ subscription. Caller must not own subscriptionMap_mutex or any listeners_mutex.
      void deactivateSubscription(const std::string& path);

      // Poll initial value via RPC call and push it into the queues
      void pollInitialValue(const std::string& path, const std::list<DoocsBackendRegisterAccessorBase*>& accessors);

      // Push error to listener
      static void pushError(DoocsBackendRegisterAccessorBase* listener, const std::string& message);

      /** static flag if dmsg_start() has been called already, with mutex for thread safety */
      bool dmsgStartCalled{false};
      std::mutex dmsgStartCalled_mutex;

      /// Structure describing a single subscription
      struct Subscription {
        Subscription() : zqmThreadId(ZMQSubscriptionManager::pthread_t_invalid) {}

        /// list of accessors listening to the ZMQ subscription.
        /// Accessing this vector requires holding zmq_callback_extra_listeners_mutex
        /// It's ok to use plain pointers here, since accessors will unsubscribe themselves in their destructor
        std::vector<DoocsBackendRegisterAccessorBase*> listeners;

        /// Mutex for zmq_callback_extra_listeners
        std::mutex listeners_mutex;

        /// Thread ID of the ZeroMQ subscription thread which calls zmq_callback. This is a DOOCS thread and hence
        /// outside our control. We store the ID inside zmq_callback so we can check whether the thread has been
        /// properly terminated in the cleanup phase. listeners_mutex must be held while accessing this variable.
        pthread_t zqmThreadId;

        /// Flag whether the subscription is currently active, i.e. whether the actual DOOCS subscription has been made.
        /// This is used to implement activateAsyncRead() properly. listeners_mutex must be held while accessing this
        /// variable.
        bool active{false};

        /// Flag whether the callback function has already been called for this subscription, with a condition variable
        /// for the notification when the callback is called for the first time.
        bool started{false};
        std::condition_variable startedCv{};

        /// Set when the first non-error value is received by the callback function. Will be cleared again when the
        /// subscription has seen a DOOCS error first hand. This is used to control whether an initial value needs to
        /// be polled by the backend or if this is expected from the DOOCS ZMQ thread.
        /// Note that the initial value from DOOCS is sent after DOOCS-internal recovery, which is independent of the
        /// exception state of the backend and its recovery!
        /// Access requires holding the listeners_mutex.
        bool gotInitialValue{false};
      };

      /// A "random" value of pthread_t which we consider "invalid" in context of Subscription::zqmThreadId. Technically
      /// we use a valid pthread_t of a thread which cannot be a ZeroMQ subscription thread. All values of
      /// Subscription::zqmThreadId will be initialised with this value.
      static pthread_t pthread_t_invalid;

      /// map of subscriptions
      std::map<std::string, Subscription> subscriptionMap;

      /// mutex for subscriptionMap
      std::mutex subscriptionMap_mutex;

      /// callback function for ZeroMQ
      /// This is a static function so we can pass a plain pointer to the DOOCS
      /// client. The first argument will contain the pointer to the object (will be
      /// statically casted into DoocsBackendRegisterAccessor<UserType>*).
      static void zmq_callback(void* self, EqData* data, dmsg_info_t* info);
    };

  } // namespace DoocsBackendNamespace

} // namespace ChimeraTK

#endif // CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H
