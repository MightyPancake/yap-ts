//A tour of Yet Another Programming language

// Single line commands

/*
	Comments can also span
	over multiple lines
	like this one.
*/

/*
	Declarations

	Each program/module consists of a list of defintions which can include:
	- Statements
		Note: Global statements will run only if the module is run directly.
		Running arbitrary call on import is prohibited by design. For such purposes you should add an `init` function.
	- Function declarations
	- Type declarations:
		1. Structure declarations (struct)
		2. Enum declarations (enum)
		3. Union declarations (union)

	- More?
*/

/*
	Function declarations

	There is really one way to declare a function which is presented below.
*/

// Declare a function named 'name' with two parameters: `param1` of `type1` and `param2` of `type2`. It has an empty body
fn name(type1 param1, type2 param2){
	//comment 
}

fn func(type1 param1=default1){}
// Notice how the function return type is never specified as it is inferred.

/*
	Statements

	Statements represent instruction will which be executed when a given piece of code runs.
	Below is an example of all the possible statements.

	Remember that global statements will not run unless the module is executed directly.
*/

// Most expressions are valid statements
1

// Variable declarations

// Initialization is advised!
var := 1

// if you want to declare without initial value, cast 'none' to your desired type
a := none.(int)

// If statements
// Notice how there is nothing that seperates the condition from the statement.
// You can always add `()` around the condtion
if condition
	statement

// If-else statements
if condition
	if_statement
else
	else_statement

// 


/*
	Expressions

	TODO
*/

// Basics
1+1-1*2/3%4

// Assignments are also expressions
a=0
a+=1
a-=1
a*=2
a/=2
// etc.

// Increment operators
c++ //lmao
c-- //more like it
// There is no prefix operator; just use += or -= instead:
c+=1 //increments then evaluates
c-=1

// TEST
expr.field
expr.(cast)
expr.field.(cast)
expr.(cast).field
1+(1+2)*3%4
if (1+2) 3

// Loops
while true
	do_stuff()

for i:=0, i++<10, ;
	do_stuff()

foo(bar)
foo(bar, bar)
foo()

struct test {
	int a,
	int b,
	char c = "xd",
	@char d
}

printf#
// test-{}
// test-{0, 2}
// test-{a:=12}

// {0, 2}.(test)


//TODO
/*

	Modules

	Each 
*/
// io->printf()
// "Hello world!\n"
// io->print()
// str = string->new("Hello!")
// str:replace("!", " world!")


// str
// |> a, count("o")
// |> append()
// |>$
