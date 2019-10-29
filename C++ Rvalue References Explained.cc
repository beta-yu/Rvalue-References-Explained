//右值引用解决两件事情：1.移动语义; 2.完美转发.

//区分左值右值:
//Here is an alternate definition which, although it can still be argued with, will put you in a position to tackle rvalue references: An lvalue is an expression that refers to a memory location and allows us to take the address of that memory location via the & operator. An rvalue is an expression that is not an lvalue. Examples are:

// lvalues:
//
int i = 42;
i = 43; // ok, i is an lvalue
int* p = &i; // ok, i is an lvalue
int& foo();
foo() = 42; // ok, foo() is an lvalue
int* p1 = &foo(); // ok, foo() is an lvalue

// rvalues:
//
int foobar();
int j = 0;
j = foobar(); // ok, foobar() is an rvalue
int* p2 = &foobar(); // error, cannot take the address of an rvalue
j = 42; // ok, 42 is an rvalue


/*
    Rvalue references allow a function to branch at compile time (via overload resolution) on the condition "Am I being called on an lvalue or an rvalue?"
*/
//右值引用允许函数在编译时判断传递的参数时左值还是右值。
//利用函数重载对左右值实现不同的行为。
//例如：右值，资源转移；左值，资源拷贝。

//1.Move semantics
//使用移动语义实现资源转移，省去不必要的资源拷贝

//一个规则:
/*
    if-it-has-a-name rule.
    Things that are declared as rvalue reference can be lvalues or rvalues. The distinguishing criterion is: if it has a name, then it is an lvalue. Otherwise, it is an rvalue.
*/

void func(X&& x) {
    X obj = x; 
    //x有名称，所以x是左值，这里不会调用 X(X&& x); 而是 X(const X& x);
    //x有名称所以x是左值，这样规定是因为在该语句之后，x还在它的作用域之内，
    //即有可能再次被访问，如果这里x为右值，则会被移动资源，
    //再对其进行访问将会是危险的操作。
    //而我们使用移动语义的场景是：被移动资源的对象不可能再被使用。
}

/* 掌握if-it-has-a-name规则的重要性 */
class Base {
public:
    Base(const Base& b);
    Base(Base&& b);
};

class Derived : public Base {
public:
    Derived(const Derived& d) 
        :Base(d)
    {
        //...
    }
    
    Derived(Derived&& d) 
    //  :Base(d) //错误：这里d是一个左值，将会调用基类的Base(const Base& b);
        :Base(std::move(d)) //正确，调用Base(Base&& b);符合我们的期望。
    {
        //...
    }
};


X&& func();
X x = func(); //这里会调用 X(X&& x);
//func()代表一个没有名称的右值引用

//强制移动语义

template<truename T>
void swap(T& a, T& b) {
    T tmp(a);
    a = b;
    b = tmp;
}
X a, b;
swap(a, b);
//上面实现中不存在右值，在swap中不存在移动语义。
//但是在swap中实现中使用移动语义似乎会更好，因为上面实现中无论是在拷贝构造还是赋值动作中
//，对象的资源都不会再被使用。
//可以使用库函数move将这种状态的参数转为右值，强制实现移动语义：
//In C++11, there is an std library function called std::move that comes to our rescue. It is a function that turns its argument into an rvalue without doing anything else. 
//Therefore, in C++11, the std library function swap looks like this:
template<truename T>
void swap(T& a, T& b) {
    T tmp(std::move(a));
    a = std::move(b);
    b = std::move(tmp);
}

//移动语义与编译器优化
//可能会存在的编译器优化(g++8是这样，Visual Studio 2019似乎并不是这样)：
X foo() {
    X x;
    //Maybe do something to x.
    return x;
    //由于编译器优化，这里不会发生资源拷贝，不会调用X的拷贝构造。
    //而是编译器直接在foo()的返回值(临时对象)出构造x，省去了函数返回时的拷贝。
}

//相反如果改为这样：
X foo() {
    X x;
    //Maybe do something to x.
    return move(x);
    //这里会去调用X(X&& x)去移动构造出临时对象; 函数内会构造出x，而不是返回值处。
    //反而比上面不使用移动语义更加冗余。
}
//似乎想要高效的使用移动语义并不简单，反而很麻烦

/***************************************************************************************/

//2. Perfect Forwarding
template<typename T, typename Arg>
shared_ptr<T> factory(Arg arg) {
    return shared_ptr<T>(new T(arg));
}
//实现上面函数的目的是使用Arg类型参数构造T类型对象并使用shared_ptr来进行资源管理。
//就arg而言，一切应该像没调用factory一样，就像是在代码中直接使用函数实现体。
//但上面实现可不是这样，arg采用了值传递，此时，如果T的构造函数采用引用的方式使用arg,
//就会出现错误。

//第二种实现方式
template<typename T, typename Arg>
shared_ptr<T> factory(Arg& arg) {
    return shared_ptr<T>(new T(arg));
}
//这是就不会发生值拷贝，即使构造函数以引用的方式使用arg，也是引用arg本身。
//但是，不能在右值上调用该函数：
factory<X>(55);
factory<X>(hoo());

//完美转发的实现:

//C++11引用折叠规则:
A& & becomes A&
A& && becomes A&
A&& & becomes A&
A&& && becomes A&&
//特殊的模板函数推演规则:
template<typename T>
void foo(T&&) //通过对模板参数的右值引用来获取参数
//(1).当在A类型的左值上调用foo，则T被推演为A&，根据引用折叠规则，
//  函数参数实际类型为A&(A& &&-->A&)。
//(2).当在A类型的右值上调用foo，则T被推演为A，则函数参数实际类型为A&&。

//使用完美转发实现factory
template<typename T, typename Arg> 
shared_ptr<T> factory(Arg&& arg)
{ 
  return shared_ptr<T>(new T(std::forward<Arg>(arg)));
} 
//forward函数模板如下:
template<class S>
S&& forward(typename remove_reference<S>::type& a) noexcept
{
  return static_cast<S&&>(a);
} 
//(1). lvalue
X x;
factory<A>(x);
//此时Arg被推演为X&，则编译器创建factory和forward的实现如下:
shared_ptr<A> factory(X& && arg)
{ 
  return shared_ptr<A>(new A(std::forward<X&>(arg)));
} 
X& && forward(typename remove_reference<X&>::type& a) noexcept
{
  return static_cast<X& &&>(a);
} 
//根据引用折叠规则，即:
shared_ptr<A> factory(X& arg)
{ 
  return shared_ptr<A>(new A(std::forward<X&>(arg)));
} 
X& forward(typename remove_reference<X&>::type& a) noexcept
{
  return static_cast<X&>(a);
} 
//因此当x被传递给A的构造函数时是以左值引用的方式

//(2). rvalue
X foo();
factory<A>(foo());
//根据规则，Arg被推演为X，即:
shared_ptr<A> factory(X&& arg)
{ 
  return shared_ptr<A>(new A(std::forward<X>(arg)));
} 
X&& forward(typename remove_reference<X>::type& a) noexcept
{
  return static_cast<X&&>(a);
} 
//此时传递给A的构造函数的参数是以右值引用的方式传递了X类型对象。

/*利用特殊的引用折叠规则与模板函数推演规则实现完美转发。*/