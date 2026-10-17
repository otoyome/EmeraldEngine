#pragma once


//	ワールドごとに実体を持つシングルトン
#define	WORLD_SINGLETON( T )															\
	SINGLETON( T )																		\
public:																					\
	static	inline	T*		create_detached_instance()	{ return new T; }				\
	static	inline	void	destroy_detached_instance(T* target) { delete target; }		\
	static	inline	void	set_current_instance(T* target) { instance_ptr = target; }	\
private:
