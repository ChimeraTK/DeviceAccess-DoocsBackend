#include "ZMQSubscriptionManager.h"
#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK { namespace DoocsBackendNamespace {

  /******************************************************************************************************************/

  pthread_t ZMQSubscriptionManager::pthread_t_invalid;

  /******************************************************************************************************************/

  ZMQSubscriptionManager::ZMQSubscriptionManager() { pthread_t_invalid = pthread_self(); }

  /******************************************************************************************************************/

  ZMQSubscriptionManager::~ZMQSubscriptionManager() {
    for(auto subscription = subscriptionMap.begin(); subscription != subscriptionMap.end();) {
      auto subscriptionToWork = subscription++;
      for(auto accessor = subscriptionToWork->second.listeners.rbegin();
          accessor != subscriptionToWork->second.listeners.rend();) {
        auto accessorToWork = accessor++;
        unsubscribe(subscriptionToWork->first, *accessorToWork);
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // create subscription if not yet existing
    if(subscriptionMap.find(path) == subscriptionMap.end()) {
      std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
      if(subscriptionsActive) {
        activate(path);
      }
    }

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

    // add accessor to list of listeners
    subscriptionMap[path].listeners.push_back(accessor);
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // ignore if no subscription exists
    if(subscriptionMap.find(path) == subscriptionMap.end()) return;

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

    // ignore if subscription exists but not for this accessor
    if(std::find(subscriptionMap[path].listeners.begin(), subscriptionMap[path].listeners.end(), accessor) ==
        subscriptionMap[path].listeners.end())
      return;

    // remove accessor from list of listeners
    subscriptionMap[path].listeners.erase(
        std::remove(subscriptionMap[path].listeners.begin(), subscriptionMap[path].listeners.end(), accessor));

    // if no listener left, delete the subscription
    if(subscriptionMap[path].listeners.empty()) {
      // temporarily unlock the locks which might block the ZQM subscription thread
      listeners_lock.unlock();
      lock.unlock();

      // remove ZMQ subscription. This will also join the ZMQ subscription thread
      deactivate(path);

      // obtain locks again
      lock.lock();

      // remove subscription from map
      subscriptionMap.erase(subscriptionMap.find(path));
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activate(const std::string& path) {
    // do nothing if already active
    if(subscriptionMap[path].active) return;

    // subscribe to property
    EqData dst;
    EqAdr ea;
    ea.adr(path);
    dmsg_t tag;
    int err = dmsg_attach(&ea, &dst, (void*)&(subscriptionMap[path]), &zmq_callback, &tag);
    if(err) {
      /// FIXME put error into queue of all accessors!
      throw ChimeraTK::runtime_error(
          std::string("Cannot subscribe to DOOCS property '" + path + "' via ZeroMQ: ") + dst.get_string());
    }

    // run dmsg_start() once
    std::unique_lock<std::mutex> lck(dmsgStartCalled_mutex);
    if(!dmsgStartCalled) {
      dmsg_start();
      dmsgStartCalled = true;
    }

    // set active flag, reset hasException flag
    subscriptionMap[path].active = true;
    subscriptionMap[path].hasException = false;
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivate(const std::string& path) {
    // do nothing if already inactive
    {
      std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
      std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);
      if(!subscriptionMap[path].active) return;

      // clear active flag
      subscriptionMap[path].active = false;
    }

    // remove ZMQ subscription. This will also join the ZMQ subscription thread
    EqAdr ea;
    ea.adr(path);
    dmsg_detach(&ea, nullptr); // nullptr = remove all subscriptions for that address
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activateAll() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
    std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
    subscriptionsActive = true;

    for(auto& subscription : subscriptionMap) {
      std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
      activate(subscription.first);
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivateAllAndPushException() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
    std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
    subscriptionsActive = false;

    for(auto& subscription : subscriptionMap) {
      // decativate subscription
      lock.unlock();
      deactivate(subscription.first);
      lock.lock();

      // put exception to queue
      {
        std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
        if(subscription.second.hasException) continue;
        subscription.second.hasException = true;
        for(auto& listener : subscription.second.listeners) {
          try {
            throw ChimeraTK::runtime_error("Exception reported by another accessor.");
          }
          catch(...) {
            listener->notifications.push_overwrite_exception(std::current_exception());
          }
        }
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::zmq_callback(void* self_, EqData* data, dmsg_info_t* info) {
    // obtain pointer to subscription object
    auto* subscription = static_cast<ZMQSubscriptionManager::Subscription*>(self_);

    // Make sure the stamp is used from the ZeroMQ header. TODO: Is this really wanted?
    data->time(info->sec, info->usec);
    data->mpnum(info->ident);

    // store thread id of the thread calling this function, if not yet done
    std::unique_lock<std::mutex> lock(subscription->listeners_mutex);
    if(pthread_equal(subscription->zqmThreadId, pthread_t_invalid)) {
      subscription->zqmThreadId = pthread_self();
    }

    // check for error
    if(data->error() != no_connection) {
      // no error: push the data
      for(auto& listener : subscription->listeners) {
        listener->notifications.push_overwrite(*data);
      }
    }
    else {
      // no error: push the data
      try {
        throw ChimeraTK::runtime_error("ZeroMQ connection interrupted: " + data->get_string());
      }
      catch(...) {
        subscription->hasException = true;
        for(auto& listener : subscription->listeners) {
          listener->notifications.push_overwrite_exception(std::current_exception());
          lock.unlock();
          listener->_backend->informRuntimeError(listener->_path);
          lock.lock();
        }
      }
    }
  }

}} // namespace ChimeraTK::DoocsBackendNamespace
