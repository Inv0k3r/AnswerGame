# AnswerGame

一个Linux下的用c语言编写的答题程序，基于socket

1. Put the file named "question_answer" and the file "server" together
2. question_answer format follow it:

    line num        content				example
        0		question and answer number	3
    1		question1			this is a question
    2		score				10
    3		answer number			4
    4		answer				answer1
    5						answer2
    6						answer3
    7						answer4
    8		true answer start for 0		1
    8		question2			this is a question2
    .....
