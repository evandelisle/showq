/*
    Copyright (C) 2000-2007 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef pbd_rcu_h_
#define pbd_rcu_h_

#include <atomic>
#include <list>
#include <memory>
#include <mutex>

template<class T>
class RCUManager
{
public:
  RCUManager(T *new_rcu_value)
    : m_rcu_value(new std::shared_ptr<T>(new_rcu_value))
  {
  }

  virtual ~RCUManager() { delete m_rcu_value; }

  std::shared_ptr<T> reader() const { return *m_rcu_value.load(); }

  virtual std::shared_ptr<T> write_copy() = 0;
  virtual bool update(std::shared_ptr<T> new_value) = 0;

protected:
  std::atomic<std::shared_ptr<T> *> m_rcu_value;
};

template<class T>
class SerializedRCUManager : public RCUManager<T>
{
public:
  SerializedRCUManager(T* new_rcu_value)
    : RCUManager<T>(new_rcu_value)
  {
  }

  std::shared_ptr<T> write_copy() override
  {
    m_lock.lock();

    // clean out any dead wood

    typename std::list<std::shared_ptr<T> >::iterator i;

    for (i = m_dead_wood.begin(); i != m_dead_wood.end(); ) {
      if ((*i).use_count() == 1) {
        i = m_dead_wood.erase (i);
      } else {
        ++i;
      }
    }

    // store the current

    current_write_old = RCUManager<T>::m_rcu_value;

    std::shared_ptr<T> new_copy(new T(**current_write_old));

    return new_copy;
  }

  bool update(std::shared_ptr<T> new_value) override
  {
    // we hold the lock at this point effectively blocking
    // other writers.

    std::shared_ptr<T> *new_spp = new std::shared_ptr<T>(new_value);

    // update, checking that nobody beat us to it

    bool ret = RCUManager<T>::m_rcu_value.compare_exchange_strong(current_write_old, new_spp);

    if (ret) {

      // successful update : put the old value into dead_wood,

      m_dead_wood.push_back(*current_write_old);

      // now delete it - this gets rid of the shared_ptr<T> but
      // because dead_wood contains another shared_ptr<T> that
      // references the same T, the underlying object lives on

      delete current_write_old;
    }

    m_lock.unlock();

    return ret;
  }

  void flush()
  {
    std::lock_guard<std::mutex> lm(m_lock);
    m_dead_wood.clear();
  }

private:
  std::mutex m_lock;
  std::shared_ptr<T> *current_write_old = nullptr;
  std::list<std::shared_ptr<T>> m_dead_wood;
};

template<class T>
class RCUWriter
{
public:
  RCUWriter(RCUManager<T>& manager)
    : m_manager(manager)
  {
    m_copy = m_manager.write_copy();
  }

  ~RCUWriter()
  {
    // we can check here that the refcount of m_copy is 1

    if (m_copy.use_count() == 1) {
      m_manager.update(m_copy);
    } else {
      // critical error.
    }
  }

  // or operator std::shared_ptr<T> ();
  std::shared_ptr<T> get_copy() { return m_copy; }

private:
  RCUManager<T>& m_manager;

  // preferably this holds a pointer to T
  std::shared_ptr<T> m_copy;
};

#endif // pbd_rcu_h_

