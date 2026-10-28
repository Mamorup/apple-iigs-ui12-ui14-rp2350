# Публикация на GitHub

Этот документ описывает первоначальную публикацию проекта и подготовку новых версий.
Репозиторий проекта: https://github.com/Mamorup/apple-iigs-ui12-ui14-rp2350

1. Открой https://github.com/new под своим аккаунтом.
2. Название: `apple-iigs-ui12-ui14-rp2350`.
3. Описание: `Experimental Apple IIgs UI12/UI14 replacement using RP2350, with ADB keyboard support.`
4. Для открытого проекта выбери Public. README, .gitignore и LICENSE уже
   находятся здесь, поэтому не создавай дополнительные шаблоны на сайте.
5. Нажми Create repository, затем uploading an existing file / Add file → Upload files.
6. Перетащи СОДЕРЖИМОЕ подготовленной папки, сохраняя вложенные каталоги.
   ZIP как единственный файл не заменяет публикацию исходников.
7. Сообщение первого коммита: `Publish RP2350 UI12/UI14 prototype with warm reset and watchdog`.
8. Проверь, что README отображается в корне, ссылки открываются, а исходники
   находятся в firmware/. В Actions можно посмотреть результат программных тестов.
9. Для первого Release можно использовать тег `v0.1.0-prototype`, включить
   отметку pre-release и приложить UF2, SHA256SUMS и manifest.json из releases/.

Лицензия MIT скопирована из рабочего проекта; сторонние уведомления сохранены.
Готовых ROM, сторонних PDF, SDK, компиляторов и больших захватов в папке нет.
Папка firmware/build-rp2350 может появиться после локальной проверки сборки:
её НЕ загружать. В ZIP для публикации сборочные файлы не включаются.

Для публикации через Git вместо сайта:

```sh
git init -b main
git add .
git commit -m "Publish RP2350 UI12/UI14 prototype with warm reset and watchdog"
git remote add origin https://github.com/YOUR-LOGIN/apple-iigs-ui12-ui14-rp2350.git
git push -u origin main
```

Здесь YOUR-LOGIN нужно заменить своим именем на GitHub. Имя/почта автора Git
настраиваются владельцем; учётные данные и токены в проект не добавляются.

Инструкции GitHub:
- https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository
- https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository
