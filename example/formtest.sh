#!/bin/ksh

printf "
<html>
	<head>
		<meta charset=\"UTF-8\">
	</head>
	<body>
		<form action=\"readpost.sh\" method=\"post\">
			<input type=\"text\" name=\"text1\">
			<input type=\"password\" name=\"pass\">
			<input type=\"reset\">
			<input type=\"radio\" name=\"radio1\" value=\"1\">
			<input type=\"radio\" name=\"radio1\" value=\"2\" checked>
			<input type=\"radio\" name=\"radio1\" value=\"3\">
			<input type=\"checkbox\" name=\"check1\" value=\"a\"> a
			<input type=\"checkbox\" name=\"check2\" value=\"b\"> b
			<textarea name=\"textarea1\">type something!</textarea>
			<select name=\"select1\">
				<option value=\"I\">I</option>
				<option value=\"III\">II</option>
				<option value=\"III\">III</option>
			</select>
			<input type=\"submit\" value=\"Отправить\">
		</form>
	</body>
</html>
";
