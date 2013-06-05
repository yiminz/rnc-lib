/* -*- mode: c++; coding: utf-8-unix -*-
 *
 * Copyright 2013 MTA SZTAKI
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/**
   \file
   \brief Test finite field operations.
*/

#include <test.h>
#include <rnc>
#include <iostream>
#include <list>
#include <stdlib.h>

using namespace std;
using namespace rnc::test;
using namespace rnc::fq;

fq_t getrand() { return rand() % fq_size; }
fq_t getrand_notnull()
{
        dq_t a;
        do { a = getrand(); } while (a==0);
        return a;
}

template <bool (*pt)(ostream*buffer)>
class FQ_TestCase : public TestCase
{
public:
        FQ_TestCase(const string &tname, size_t n)
                : TestCase(string("fq::") + tname, n) {}
        bool performTest(ostream *buffer) const
        {
                return pt(buffer);
        }
};

#define new_TC(a, n) new FQ_TestCase<a>(#a, n)

bool inv_1(ostream *buffer)
{
        if (buffer)
                (*buffer) << "log_table[1] = " << log_table[1];
        return inv(1) == 1;
}
bool inv_2(ostream *buffer)
{
        fq_t a = getrand_notnull();
        if (buffer)
        {
                (*buffer) << "a=";
                p(a, *buffer);
        }
        return 1 == mul(a, inv(a));
}
bool invert_1(ostream *buffer)
{
        fq_t a = getrand_notnull();
        fq_t t = a;
        invert(t);
        return inv(a) == t;
}

bool mul_1(ostream *buffer)
{
        fq_t a = getrand();
        return a == mul(1, a);
}

bool mul_2(ostream *buffer)
{
        fq_t a = getrand_notnull();
        fq_t b = getrand_notnull();
        return mul(a,b) == mul(a, b);
}

bool mul_3(ostream *buffer)
{
        fq_t a = getrand_notnull();
        fq_t b = getrand_notnull();
        fq_t c = getrand_notnull();
        return mul(a, mul(b, c)) == mul(mul(a, b), c);
}

bool addto_1(ostream *buffer)
{
        fq_t a = getrand();
        fq_t b = getrand();
        fq_t t = a;
        addto(t, b);
        return t == add(a,b);
}

bool addto_mul_1(ostream *buffer)
{
        fq_t a = getrand();
        return a == mul(1, a);
}


bool div_1(ostream *buffer)
{
        fq_t a = getrand();
        fq_t b = getrand_notnull();
        return a == mul(b, div(a,b));
}

bool divby_1(ostream *buffer)
{
        fq_t a = getrand();
        fq_t b = getrand_notnull();
        fq_t t = a;
        divby(t, b);
        return t == div(a,b);
}

int main(int argc, char **argv)
{
        srand(time(NULL));

        init();

        cout << fq_size << endl;

        typedef list<TestCase*> case_list;
        case_list cases;
        cases.push_back(new_TC(inv_1, 1));
        cases.push_back(new_TC(inv_2, 5));

        for (case_list::const_iterator i = cases.begin();
             i!=cases.end(); i++)
        {
                (*i)->execute(cout);
        }

        return 0;
}

