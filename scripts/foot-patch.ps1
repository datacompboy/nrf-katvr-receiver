$in_line = 0
$Input | ForEach-Object {
  $in_line++
  $skip = 0
  if ($in_line -eq   933) {
    if ($_ -ne ':206AF00010201E46FFF77EF8C0B218B130461B21FAF762FAE08A401C80B2B0F5FA7F18BF5F') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   934) {
    if ($_ -ne ':206B1000051CE582F8BDC04630110020EC1F0020200500204500011000010E402DE9FC4753') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   935) {
    if ($_ -ne ':206B300005466E68D5F8088037780FB1B76B1FB14FF0FF30BDE8FC8731630024F477171C82') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq   935) {
    Write-Output ':206AF00010201E46FFF77EF8C0B218B130461B21FAF762FAE08A401C80B2B0F5FA7F18BF5F'
  }
  if ($in_line -eq   935) {
    Write-Output ':206B1000051CE5820CF094B930110020EC1F0020200500204500011000010E402DE9FC47C5'
  }
  if ($in_line -eq   935) {
    Write-Output ':206B300005466E68D5F8088037780FB1B76B1FB14FF0FF30BDE8FC8731630024F477171C82'
  }
  if ($in_line -eq  1051) {
    if ($_ -ne ':2079B000381701004004002010050020000500203004002041482DE9FE4F00683D4C3E4DED') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1052) {
    if ($_ -ne ':2079D00041F65831B1FBF0F00290394800264FF0010900F1060A804668684FF0FF3108F0CB') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1053) {
    if ($_ -ne ':2079F00087FE0298F9F7F4FA08F01CF9502008F0DEF8642008F09EF9D3460C278DF80060EB') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1053) {
    Write-Output ':2079B000381701004004002010050020000500203004002041482DE9FE4F00683D4C3E4DED'
  }
  if ($in_line -eq  1053) {
    Write-Output ':2079D00041F28831B1FBF0F00290394800264FF0010900F1060A804668684FF0FF3108F09F'
  }
  if ($in_line -eq  1053) {
    Write-Output ':2079F00087FE0298F9F7F4FA08F01CF9502008F0DEF8642008F09EF9D3460C278DF80060EB'
  }
  if ($in_line -eq  1058) {
    if ($_ -ne ':207A9000FF0007F0FF07C01B022806DB6088401C80B23228D8BF608001DD218066806E805A') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1059) {
    if ($_ -ne ':207AB0002878002898D16868F9F77EFA94E7C04620050020EC1F002084150020A81B0100DF') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1060) {
    if ($_ -ne ':207AD0002DE9FC4706F060F9DFF8F490824609F1020000880027001F84B2042C51DBFF254B') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  1060) {
    Write-Output ':207A9000FF0007F0FF07C01B022806DB6088401C80B23228D8BF608001DD218066806E805A'
  }
  if ($in_line -eq  1060) {
    Write-Output ':207AB0009AE7002898D16868F9F77EFA94E7C04620050020EC1F002084150020A81B0100FE'
  }
  if ($in_line -eq  1060) {
    Write-Output ':207AD0002DE9FC4706F060F9DFF8F490824609F1020000880027001F84B2042C51DBFF254B'
  }
  if ($in_line -eq  2068) {
    if ($_ -ne ':20F8D000941B01002022EFF3118382F31188426A1143416283F3118800F0E2BC044991F88C') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2069) {
    if ($_ -ne ':20F8F0009100032814BF0420052088707047C0463011002008B50090BDF80200F1F750FBD3') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2070) {
    if ($_ -ne ':20F91000BDF8000000F024FF08BD10F07F4F08B503D1FEF74FFF002008BD6FF0020008BD9D') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2070) {
    Write-Output ':20F8D000941B01002022EFF3118382F31188426A1143416283F3118800F0E2BC044991F88C'
  }
  if ($in_line -eq  2070) {
    Write-Output ':20F8F0009100032814BF04200520887003F088BA3011002008B50090BDF80200F1F750FB5B'
  }
  if ($in_line -eq  2070) {
    Write-Output ':20F91000BDF8000000F024FF08BD10F07F4F08B503D1FEF74FFF002008BD6FF0020008BD9D'
  }
  if ($in_line -eq  2496) {
    if ($_ -ne ':1C2DE800EC010020672D0100F00100206E2D0100F8010020752D0100040200209E') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2497) {
    if ($_ -ne ':20E00000FF0000000000000000000000000000000000000000000000000000000000000001') { throw 'File content mismatch'; }
    $skip = 1
  }
  if ($in_line -eq  2497) {
    Write-Output ':1C2DE800EC010020672D0100F00100206E2D0100F8010020752D0100040200209E'
  }
  if ($in_line -eq  2497) {
    Write-Output ':202E100014BF4C205220054A90712D2050714FF44270102143F23D66B7460000A4110020C3'
  }
  if ($in_line -eq  2497) {
    Write-Output ':142E4000034D2878002802D16868EEF7B5F8F8BD84150020C3'
  }
  if ($in_line -eq  2497) {
    Write-Output ':20E00000FF0000000000000000000000000000000000000000000000000000000000000001'
  }
  if ($skip -eq 0) {
    Write-Output $_
  }
}
