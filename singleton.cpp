template<typename T>
class singleton
{
    static std::unique_ptr<T> instance_;

    static std::once_flag & get_once_flag( )
    {
        static std::once_flag flag;
        return flag;
    }

protected:
    singleton( ) = default;
    ~singleton( ) = default;

public:
    template<typename... Args>
    static T & get_instance( Args&& ...args )
    {
        std::call_once(
            get_once_flag( ),
            [] ( Args&&... args ) { instance_.reset( new T( std::forward<Args>( args )... ) ); },
            std::forward<Args>( args )... );

        return *instance_;
    }

    singleton( const singleton & ) = delete;
    singleton & operator =( const singleton & ) = delete;
    singleton( singleton && ) = delete;
    singleton & operator =( singleton && ) = delete;
};
template<typename T> std::unique_ptr<T> singleton<T>::instance_ = nullptr;
