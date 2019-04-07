if [ -z "$1" ]
then
	echo 'No argument given'
else
	pintos -f -q
	pintos -p build/tests/userprog/no-vm/$1 -a $1 -- -q

fi
