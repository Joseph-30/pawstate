$word = New-Object -ComObject Word.Application
$word.Visible = $false

$doc1 = $word.Documents.Open("c:\sample-pawstate\PawState_Startup_Roadmap.docx")
$doc1.Content.Text | Out-File -FilePath "c:\sample-pawstate\PawState_Startup_Roadmap.txt" -Encoding UTF8
$doc1.Close()

$doc2 = $word.Documents.Open("c:\sample-pawstate\pawstate_real-time-canine-emotion-activity-monitoring-via-collar-imu.docx")
$doc2.Content.Text | Out-File -FilePath "c:\sample-pawstate\pawstate_imu_doc.txt" -Encoding UTF8
$doc2.Close()

$word.Quit()
Write-Host "Done extracting documents"
