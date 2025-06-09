#ifndef ByteReader_h
#define ByteReader_h

#include <QByteArray>
#include <coroutine>

struct Task {
    struct promise_type
    {
        Task get_return_object()
        {
            return {};
        }

        std::suspend_never initial_suspend() noexcept
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            return {};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        void return_void()
        {
        }
    };
};


class	ByteReader
{

public:

	/*
	 ===================================================================================================================
	 ===================================================================================================================
	 */
    ByteReader(QByteArray &_racBuffer, int _iSize) : m_acBuffer(_racBuffer), m_iSize(_iSize)
	{
	}

    /*
     ===================================================================================================================
     ===================================================================================================================
     */
    bool await_ready () const noexcept
	{
        return m_acBuffer.size() >= m_iSize;
	}

	/*
	 ===================================================================================================================
	 ===================================================================================================================
	 */
	void await_suspend(std::coroutine_handle<> h)
	{
        m_soHandle = h;
	}

	/*
	 ===================================================================================================================
	 ===================================================================================================================
	 */
	QByteArray await_resume()
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QByteArray	data = m_acBuffer.left(m_iSize);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        m_acBuffer.remove(0, m_iSize);
		return data;
	}

	/*
	 ===================================================================================================================
	 ===================================================================================================================
	 */
	void tryResume()
	{
        if(await_ready() && m_soHandle)
		{
            m_soHandle.resume();
		}
	}

private:
    QByteArray						&m_acBuffer;
    int								m_iSize;
    static std::coroutine_handle<>	m_soHandle;
};
#endif
