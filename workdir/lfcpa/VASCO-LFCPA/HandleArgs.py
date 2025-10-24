from argparse import ArgumentParser

if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument("--NAIVE_MODE", help="Enable Naive Implementation", action="store_true")
    parser.add_argument("--SHOW_OUTPUT", help="Show Analysis Output", action="store_true")
    parser.add_argument("--PRINT", help="Print all", action="store_true")
    parser.add_argument("--FUNPTR", help="Handle function pointers", action="store_true")
    parser.add_argument("--PRINTSTATS", help="Print statistics after analysis", action="store_true")
    parser.add_argument("--SIMPLE_PRINT", help="Prints required data", action="store_true")
    parser.add_argument("--SLIMIR", help="Prints SLIM instructions", action="store_true")
    res = parser.parse_args()

    Result = " "

    if res.PRINT:
        Result += "PRINT "

    if res.FUNPTR:
        Result += "FUNPTR "

    if res.PRINTSTATS:
        Result += "PRINTSTATS "

    if res.SIMPLE_PRINT:
        Result += "SIMPLE_PRINT "

    if res.NAIVE_MODE:
        Result += "NAIVE_MODE "

    if res.SHOW_OUTPUT:
        Result += "SHOW_OUTPUT "

    print(Result)
