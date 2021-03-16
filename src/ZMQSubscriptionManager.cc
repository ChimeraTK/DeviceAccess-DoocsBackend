#include "ZMQSubscriptionManager.h"
#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK { namespace DoocsBackendNamespace {

  /******************************************************************************************************************/

  pthread_t ZMQSubscriptionManager::pthread_t_invalid;

  /******************************************************************************************************************/

  ZMQSubscriptionManager::ZMQSubscriptionManager() { pthread_t_invalid = pthread_self(); }

  /******************************************************************************************************************/

  ZMQSubscriptionManager::~ZMQSubscriptionManager() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    for(auto subscription = subscriptionMap.begin(); subscription != subscriptionMap.end();) {
      {
        std::unique_lock<std::mutex> listeners_lock(subscription->second.listeners_mutex);
        subscription->second.listeners.clear();
      }
      lock.unlock();
      deactivateSubscription(subscription->first);
      lock.lock();
      subscription = subscriptionMap.erase(subscription);
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    bool newSubscription = false;

    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // check if subscription is already in the map
    newSubscription = subscriptionMap.find(path) == subscriptionMap.end();

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

    // add accessor to list of listeners
    subscriptionMap[path].listeners.push_back(accessor);

    // subscriptionMap is no longer used below this point
    lock.unlock();

    // Set flag whether ZMQ is activated for this accessor
    accessor->isActiveZMQ = accessor->_backend->_asyncReadActivated;

    // create subscription if not yet existing. must be done after the previous steps to make sure the initial value
    // is not lost
    if(newSubscription) {
      activateSubscription(path); // just establish the ZeroMQ subscription - listeners are still deactivated
    }

    // If required, poll the initial value and push it into the queue. This must be done after the subcription has been
    // made.
    if(!newSubscription && accessor->isActiveZMQ) {
      listeners_lock.unlock(); // lock no longer required and pollInitialValue might take a while...
      pollInitialValue(path, accessor);
    }
  } // namespace DoocsBackendNamespace

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::pollInitialValue(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    EqData src, dst;
    EqAdr adr;
    EqCall eq;
    adr.adr(path);
    auto rc = eq.get(&adr, &src, &dst);
    if(rc && DoocsBackend::isCommunicationError(dst.error())) {
      // communication error: push exception
      try {
        throw ChimeraTK::runtime_error("ZeroMQ connection interrupted: " + dst.get_string());
      }
      catch(...) {
        accessor->notifications.push_overwrite_exception(std::current_exception());
      }
      accessor->_backend->informRuntimeError(path);
    }
    else {
      // no error: push data
      accessor->notifications.push_overwrite(dst);
    }
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
      deactivateSubscription(path);

      // obtain locks again
      lock.lock();

      // remove subscription from map
      subscriptionMap.erase(subscriptionMap.find(path));
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activateSubscription(const std::string& path) {
    // precondition: subscriptionMap_mutex an the subscription's listeners_mutex must be locked

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

  void ZMQSubscriptionManager::deactivateSubscription(const std::string& path) {
    // do nothing if already inactive
    {
      std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
      std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);
      if(!subscriptionMap[path].active) return;

      // Wait until we have seen any reaction from ZMQ. This is to work around a race condition in DOOCS's ZMQ
      // subcription mechanism
      while(not subscriptionMap[path].started) subscriptionMap[path].startedCv.wait(listeners_lock);

      // clear active flag
      subscriptionMap[path].active = false;
      subscriptionMap[path].started = false;
    }

    // remove ZMQ subscription. This will also join the ZMQ subscription thread
    EqAdr ea;
    ea.adr(path);
    dmsg_detach(&ea, nullptr); // nullptr = remove all subscriptions for that address
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activateAllListeners(DoocsBackend* backend) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    for(auto& subscription : subscriptionMap) {
      std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
      for(auto& listener : subscription.second.listeners) {
        if(listener->_backend.get() == backend) {
          listener->isActiveZMQ = true;
          if(!subscription.second.hasException) {
            pollInitialValue(subscription.first, listener);
          }
        }
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivateAllListeners(DoocsBackend* backend) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    for(auto& subscription : subscriptionMap) {
      for(auto& listener : subscription.second.listeners) {
        if(listener->_backend.get() == backend) {
          listener->isActiveZMQ = false;
        }
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivateAllListenersAndPushException(DoocsBackend* backend) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    for(auto& subscription : subscriptionMap) {
      // put exception to queue
      {
        std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
        if(subscription.second.hasException) continue;
        // Determine if the subscription has a listener on the affected backend. If so, we deactivate the whole subscription (to be reviewed),
        // otherwise we leave it intact.
        bool subscriptionIsForThisBackend = false;
        for(auto& listener : subscription.second.listeners) {
          if(listener->_backend.get() == backend) {
            subscriptionIsForThisBackend = true;
          }
        }
        if(not subscriptionIsForThisBackend) continue;

        subscription.second.hasException = true;
        for(auto& listener : subscription.second.listeners) {
          // Don't push exceptions into deactivated listeners.
          // The check in subscription.second.hasException is not sufficient because it is reset in open(),
          // but activateAsyncRead() might not have been called when the next setException comes in.
          if(!listener->isActiveZMQ) continue;

          // First deactivate the listener to avoid race conditions with pushing the exception. Nothing must be pushed
          // after the exception until a succefful open() and activateAsyncRead(). (see. Spec B.9.3.1 and B.9.3.2)
          listener->isActiveZMQ = false;

          // Now finally put the exception to the queue
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

    std::unique_lock<std::mutex> lock(subscription->listeners_mutex);

    // As long as we get a callback from ZMQ, we consider it started
    if(not subscription->started) {
      subscription->started = true;
      subscription->startedCv.notify_all();
    }

    // We must not push anything to the subscribers as long as the subscription has an exception.
    if(subscription->hasException) {
      return;
    }

    // store thread id of the thread calling this function, if not yet done
    if(pthread_equal(subscription->zqmThreadId, pthread_t_invalid)) {
      subscription->zqmThreadId = pthread_self();
    }

    // check for error
    if(!DoocsBackend::isCommunicationError(data->error())) {
      // data has been received: push the data
      for(auto& listener : subscription->listeners) {
        if(listener->isActiveZMQ) {
          listener->notifications.push_overwrite(*data);
        }
      }
    }
    else {
      // error: push an exception
      try {
        throw ChimeraTK::runtime_error("ZeroMQ connection interrupted: " + data->get_string());
      }
      catch(...) {
        subscription->hasException = true;
        for(auto& listener : subscription->listeners) {
          if(listener->isActiveZMQ) {
            listener->notifications.push_overwrite_exception(std::current_exception());
            lock.unlock();
            listener->_backend->informRuntimeError(listener->_path);
            lock.lock();
          }
        }
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::reActicateAllSubscriptions(DoocsBackend* backend) {
    std::unique_lock<std::mutex> subscriptionLock(subscriptionMap_mutex);

    //FIXME: This is not really clean. If there are listeners from multiple backends
    // on the same subscription the subscription is activated for all of them. As this
    // is an unlikely scenario (why would you have two devices subscribed to the same property=
    // the implementation is good enough for now. Subscriptions from other backends are not activated if
    // they don't have shared listeners.
    for(auto& subscription : subscriptionMap) {
      std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
      for(auto& listener : subscription.second.listeners) {
        if(listener->_backend.get() == backend) {
          subscription.second.hasException = false; // solution as it should be, but freezes, see issue #7880

          /* Code that is causing long delays in open(), see issue #7880
             // Due to the way the functions are used in different context deactivate locks the mutexes internally
             // while the activate requires the locks to be held already when it is called.
             auto path = subscription.first;
             listeners_lock.unlock();
             subscriptionLock.unlock();
             deactivateSubscription(path);
             subscriptionLock.lock();
             listeners_lock.lock();
             activateSubscription(path);
          */
        } // if backend
      }   // for listener
    }     // for scubscription
  }

}} // namespace ChimeraTK::DoocsBackendNamespace
