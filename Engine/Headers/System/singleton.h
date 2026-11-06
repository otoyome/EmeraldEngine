#pragma	once

#define	SINGLETON( T )									\
public:													\
	static	inline	void	create_instance()			\
	{													\
		if(!instance_ptr)								\
		{												\
			instance_ptr	= new T;					\
		}												\
	}													\
	static	inline	void	delete_instance()			\
	{													\
		if(instance_ptr)								\
		{												\
			delete instance_ptr;						\
			instance_ptr	= nullptr;					\
		}												\
	}													\
	static	inline	T*	instance()						\
	{													\
		return	instance_ptr ? instance_ptr : nullptr;	\
	}													\
	static	inline	bool	is_valid()					\
	{													\
		return	instance_ptr ? true : false;			\
	}													\
private:												\
	static	inline	T*	instance_ptr;

#define	SINGLETON_DECREATE( T )							\
public:													\
	static	inline	void	delete_instance()			\
	{													\
		if(instance_ptr)								\
		{												\
			delete instance_ptr;						\
			instance_ptr	= nullptr;					\
		}												\
	}													\
	static	inline	T*	instance()						\
	{													\
		return	instance_ptr ? instance_ptr : nullptr;	\
	}													\
	static	inline	bool	is_valid()					\
	{													\
		return	instance_ptr ? true : false;			\
	}													\
private:												\
	static	inline	T*	instance_ptr;

#define	STATIC_SINGLETON( T )		\
public:								\
	static	inline	T*	instance()	\
	{								\
		static	T	instance_data;	\
		return	&instance_data;		\
	}								\
private:

//	@tool 移行用：実体はメインのワールドが持ち、instance() はメインのワールドの物を返す
//	@tool engine_context への置き換えが済んだら削除する
#define	MAIN_WORLD_SINGLETON( T )									\
public:																\
	static	inline	T*		instance()			{ return instance_ptr; }	\
	static	inline	bool	is_valid()			{ return instance_ptr != nullptr; }	\
	static	inline	void	set_main_world_instance(T* target) { instance_ptr = target; }	\
private:															\
	static	inline	T*	instance_ptr = nullptr;
