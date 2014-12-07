# $1: number of nodes
# $2: probability of a fact


in=( 0 1 2 317 493 316 489 454 455 3 494 314 315 737 99 756 86 719 456 720 318 4 495 488 313 718 472 471 100 101 102 103 104 105 542 98 490 81 82 83 84 85 80 539 79 457 721 319 496 473 8 755 6 738 736 108 95 109 96 110 97 111 94 112 270 107 381 106 93 491 757 10 11 12 13 14 500 7 75 9 76 77 78 74 320 321 67 538 73 487 89 68 90 69 91 70 92 71 88 5 499 322 501 60 72 66 537 65 486 754 362 540 87 470 61 62 63 64 498 323 502 59 536 58 305 434 435 485 753 361 579 469 735 543 115 541 116 117 118 119 271 375 623 120 382 641 113 114 544 252 254 123 122 253 464 376 624 380 383 642 121 445 446 249 609 745 251 545 126 125 250 465 729 377 625 294 640 384 643 124 311 710 447 711 127 546 292 296 295 297 639 246 608 248 174 129 128 247 444 312 709 448 712 130 547 300 708 458 449 713 243 607 245 132 131 244 175 290 293 638 301 463 302 133 548 134 135 136 549 137 138 139 550 141 173 140 142 551 144 143 145 552 147 146 148 553 150 149 151 554 56 153 152 154 555 156 155 157 556 159 158 160 557 15 16 325 326 506 378 324 505 327 507 626 379 627 504 328 508 622 503 329 509 162 161 163 558 165 164 166 559 168 167 169 560 171 17 170 561 21 18 19 20 22 512 220 219 221 598 223 222 224 599 226 225 227 172 580 177 178 179 176 581 256 255 257 55 181 610 182 183 698 51 57 535 54 180 582 259 258 260 185 611 186 187 600 229 184 583 262 261 263 189 612 190 191 188 584 193 194 195 192 585 269 198 268 267 615 196 197 586 201 266 265 614 228 200 199 587 204 264 613 203 202 588 207 206 205 589 209 210 722 230 208 590 212 213 211 591 215 216 214 592 217 231 601 232 233 234 602 235 236 237 603 238 239 240 604 241 23 24 25 513 26 27 514 28 29 515 30 31 516 242 605 606 32 33 517 34 518 466 730 467 731 272 273 274 628 275 393 276 629 277 394 654 278 630 279 395 655 280 631 281 396 656 282 632 283 284 633 285 286 634 287 288 635 289 636 291 637 298 653 462 728 399 299 652 461 727 303 659 460 726 474 475 443 304 400 660 740 476 741 442 707 739 477 742 441 706 478 743 497 440 705 401 661 744 402 662 403 663 306 307 665 309 308 432 664 310 696 452 433 431 695 436 437 453 716 666 430 694 699 700 438 701 717 429 693 439 702 703 459 723 724 725 330 510 331 511 492 335 39 41 522 37 40 521 336 42 523 38 520 43 524 44 337 525 46 45 526 47 527 48 49 528 50 338 529 339 530 340 531 341 532 53 533 52 534 374 479 373 621 480 746 367 342 343 562 344 563 345 564 346 565 347 566 348 567 349 568 350 569 351 570 352 571 572 35 354 357 358 574 359 575 360 576 577 578 363 593 364 594 365 595 366 596 597 369 370 616 371 617 372 618 619 620 385 644 386 645 387 646 388 647 389 648 390 649 650 391 651 392 657 397 658 398 404 405 667 406 668 407 669 408 670 409 671 410 672 411 673 412 674 413 675 414 676 415 677 416 678 417 679 418 680 419 681 420 682 421 683 422 684 423 685 424 686 425 687 426 688 427 689 428 690 691 692 704 450 714 451 715 697 468 732 733 734 481 747 482 748 483 749 484 750 751 752 758 759 761 762 ) 


out=( 0 1 2 317 493 316 489 454 455 3 494 314 315 737 99 756 86 719 456 720 318 4 495 488 313 718 472 471 100 101 102 103 104 105 542 98 490 81 82 83 84 85 80 539 79 457 721 319 496 473 8 755 6 738 736 108 95 109 96 110 97 111 94 112 270 107 381 106 93 491 757 10 11 12 13 14 500 7 75 9 76 77 78 74 320 321 67 538 73 487 89 68 90 69 91 70 92 71 88 5 499 322 501 60 72 66 537 65 486 754 362 540 87 470 61 62 63 64 498 323 502 59 536 58 305 434 435 485 753 361 579 469 735 543 115 541 116 117 118 119 271 375 623 120 382 641 113 114 544 252 254 123 122 253 464 376 624 380 383 642 121 445 446 249 609 745 251 545 126 125 250 465 729 377 625 294 640 384 643 124 311 710 447 711 127 546 292 296 295 297 639 246 608 248 174 129 128 247 444 312 709 448 712 130 547 300 708 458 449 713 243 607 245 132 131 244 175 290 293 638 301 463 302 133 548 134 135 136 549 137 138 139 550 141 173 140 142 551 144 143 145 552 147 146 148 553 150 149 151 554 56 153 152 154 555 156 155 157 556 159 158 160 557 15 16 325 326 506 378 324 505 327 507 626 379 627 504 328 508 622 503 329 509 162 161 163 558 165 164 166 559 168 167 169 560 171 17 170 561 21 18 19 20 22 512 220 219 221 598 223 222 224 599 226 225 227 172 580 177 178 179 176 581 256 255 257 55 181 610 182 183 698 51 57 535 54 180 582 259 258 260 185 611 186 187 600 229 184 583 262 261 263 189 612 190 191 188 584 193 194 195 192 585 269 198 268 267 615 196 197 586 201 266 265 614 228 200 199 587 204 264 613 203 202 588 207 206 205 589 209 210 722 230 208 590 212 213 211 591 215 216 214 592 218 368 519 231 601 232 233 234 602 235 236 237 603 238 239 240 604 241 23 24 25 513 26 27 514 28 29 515 30 31 516 242 605 606 32 33 517 34 332 518 466 730 467 731 272 273 274 628 275 393 276 629 277 394 654 278 630 279 395 655 280 631 281 396 656 282 632 283 284 633 285 286 634 287 288 635 289 636 291 637 298 653 462 728 399 299 652 461 727 303 659 460 726 474 475 443 304 400 660 740 476 741 442 707 739 477 742 441 706 478 743 497 440 705 401 661 744 402 662 403 663 306 307 665 309 308 432 664 310 696 452 433 431 695 436 437 453 716 666 430 694 699 700 438 701 717 429 693 439 702 703 459 723 724 725 330 510 331 511 492 335 39 41 522 37 40 521 336 42 523 38 520 43 524 44 337 525 46 45 526 47 527 48 49 528 50 338 529 339 530 340 531 341 532 53 533 52 534 374 479 373 621 480 746 367 342 343 562 344 563 345 564 346 565 347 566 348 567 349 568 350 569 351 570 352 571 353 572 333 334 36 355 356 573 357 358 574 359 575 360 576 577 578 363 593 364 594 365 595 366 596 597 369 370 616 371 617 372 618 619 620 385 644 386 645 387 646 388 647 389 648 390 649 650 391 651 392 657 397 658 398 404 405 667 406 668 407 669 408 670 409 671 410 672 411 673 412 674 413 675 414 676 415 677 416 678 417 679 418 680 419 681 420 682 421 683 422 684 423 685 424 686 425 687 426 688 427 689 428 690 691 692 704 450 714 451 715 697 468 732 733 734 481 747 482 748 483 749 484 750 751 752 760 )

propin=$((32768 * $2 / 100))


if [[ $propin -le 20 ]]; then
	temp=$(($2*4))
	propout=$((32768 * $temp / 100)) 
elif [[ $propin -le 30 ]]; then 
	temp=$(($2*3))
        propout=$((32768 * $temp / 100))
elif [[ $propin -le 40 ]]; then
	temp=$((($4*2)+10))
        propout=$((32768 * $temp / 100))
else 
	propout=32768
fi

for i in "${in[@]}" 
do
	if [[ $RANDOM -le $propin ]]; then
		echo "in(\"n$i\")."
	fi
done

 for i in "${out[@]}" 
 do
        if [[ $RANDOM -le $propout ]]; then
                 echo "out(\"n$i\")."
        fi
 done
