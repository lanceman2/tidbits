#!/bin/bash


echo -e "Content-type: text/html\n"

cat << EOF
<!DOCTYPE html>
<html lang=en>
<head>
    <meta charset="utf-8"/>
    <title>lanceman</title>
    <style>
        body {
            background-color: #8FBEEF;
        }
    </style>
</head>
<body>

    <p>Welcome to the edge of the earth.</p>

    <p>We have an environment variable OOD_USER_ENV=$OOD_USER_ENV</p>

</body>
</html>
EOF

