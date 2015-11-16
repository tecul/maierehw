#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "core.h"
#include "fix.h"

struct pseudo_info {
    double time[4];
    double distance[4];
};

struct pos {
    double x;
    double y;
    double z;
    double b;//only for user
    double distance_correction; //only for sat
};

struct fix {
    struct pseudo_info pseudo;
    struct pos user;
    struct pos sat[4];
};

static int gluInvertMatrix(const double m[16], double invOut[16])
{
    double inv[16], det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return 0;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return 1;
}

static void find_pseudo(struct pseudo_info *pseudo, struct pvt_info *info)
{
    int i;

    for(i = 0; i < 4; i++) {
        pseudo->time[i] = (info->timestamp[i] - info->timestamp[0]);
        pseudo->distance[i] = pseudo->time[i] * c;
    }
}

static double compute_transit_time(struct pos *pos1, struct pos *pos2)
{
    return sqrt(pow(pos1->x - pos2->x, 2) + pow(pos1->y - pos2->y, 2) + pow(pos1->z - pos2->z, 2)) / c;
}

static double compute_E(double M, double es)
{
    int i;
    double E = M;

    for(i = 0; i < 10; ++i)
        E = M + es * sin(E);

    return E;
}

static void compute_sat_pos(double gps_time, struct eph *eph, struct pos *user, struct pos *sat)
{
    double tc;
    double n;
    double as;
    double M;
    double E;
    double delta_tr;
    double delta_t;
    double t;
    double v1, v2, v;
    double phi;
    double r;
    double dphi, dr, di;
    double i;
    double omega_er;
    double x, y, z;

    /* compute n */
    as = eph->sf2.square_root_as * eph->sf2.square_root_as;
    n = sqrt(mu / (as * as *as)) + eph->sf2.delta_n;

    /* we compute satellite pos at time of transmition */
    tc = gps_time - compute_transit_time(user, sat);
    if (tc - eph->sf2.toe > 302400)
        tc = tc - 604800;
    if (tc - eph->sf2.toe < -302400)
        tc = tc + 604800;

    /* mean anomaly */
    M = eph->sf2.m0 + n * (tc - eph->sf2.toe);

    /* eccentric anomaly */
    E = compute_E(M, eph->sf2.es);

    /* relativistic correction */
    delta_tr = F * eph->sf2.es * eph->sf2.square_root_as * sin(E);

    /* delta_t */
    delta_t = eph->sf1.af0 +
              eph->sf1.af1 * (tc - eph->sf1.toc) +
              eph->sf1.af2 * (tc - eph->sf1.toc) * (tc - eph->sf1.toc) +
              delta_tr - eph->sf1.tgd;

    /* t */
    t = tc - delta_t;

    /* true anomaly */
    v1 = acos((cos(E) - eph->sf2.es) / (1 - eph->sf2.es * cos(E)));
    v2 = asin((sqrt(1 - eph->sf2.es * eph->sf2.es) * sin(E)) / (1 - eph->sf2.es * cos(E)));
    v = v2>=0?v1:-v1;

    /* phi */
    phi = eph->sf3.omega + v;

    /* compute r */
    r = as * (1 - eph->sf2.es * cos(E));

    /* correction term */
    dphi = eph->sf2.cus * sin(2*phi) + eph->sf2.cuc * cos(2*phi);
    dr = eph->sf2.crs * sin(2*phi) + eph->sf3.crc * cos(2*phi);
    di = eph->sf3.cis * sin(2*phi) + eph->sf3.cic * cos(2*phi);

    phi += dphi;
    r+= dr;
    i = eph->sf3.io + di + eph->sf3.idot * (t - eph->sf2.toe);

    /* omega_er at time of reception */
    omega_er = eph->sf3.omega_e + eph->sf3.omega_rate * (gps_time - eph->sf2.toe) - omega_ie *gps_time;

    /* solution */
    x = r * cos(omega_er) * cos(phi) - r * sin(omega_er) * cos(i) * sin(phi);
    y = r * sin(omega_er) * cos(phi) + r * cos(omega_er) * cos(i) * sin(phi);
    z = r * sin(i) * sin(phi);

    sat->x = x;
    sat->y = y;
    sat->z = z;
    sat->distance_correction = delta_t * c;
}

static void dump_sat_pos(struct pos *sat)
{
    double d = sqrt(pow(sat->x,2) + pow(sat->y,2) + pow(sat->z,2));
    printf("%.15g : %.15g %.15g %.15g %.15g\n", d, sat->x, sat->y, sat->z, sat->distance_correction);
}

static double compute_pseudo(double x, double y, double z, struct pos *sat)
{
    //printf("xu = %.15lg / yu = %.15lg / zu = %.15lg\n", x, y ,z);
    //printf("xs = %.15lg / ys = %.15lg / zs = %.15lg\n", sat->x, sat->y ,sat->z);
    return sqrt(pow(x - sat->x, 2) + pow(y - sat->y, 2) + pow(z - sat->z, 2));
}

static void compute_user_pos(struct pos *pos, struct pos *sat, struct pseudo_info *pseudo)
{
    double xu, yu, zu, bu;
    double dxu, dyu, dzu, dbu;
    double p[4];
    double dp[4];
    double alpha[4][4];
    double inv[4][4];
    double res[4][4];
    int i;
    int k;

    xu = pos->x;
    yu = pos->y;
    zu = pos->z;
    bu = pos->b;
    //int r;

    for(k = 0; k < 6; k++) {
        //printf("posu = %.15g / %.15g / %.15g | %.15g\n", xu, yu, zu, bu);
        for(i = 0; i < 4; i++) {
            p[i] = compute_pseudo(xu, yu, zu, &sat[i]);
            dp[i] = pseudo->distance[i] + sat[i].distance_correction - p[i] - bu;
            //printf("%.15lg = %.15lg + %.15lg + %.15lg + %.15lg\n", dp[i], pseudo->distance[i] ,sat[i].distance_correction ,p[i] ,bu);

            alpha[i][0] = (-sat[i].x + xu) / (p[i]);
            alpha[i][1] = (-sat[i].y + yu) / (p[i]);
            alpha[i][2] = (-sat[i].z + zu) / (p[i]);
            alpha[i][3] = 1.0;
        }
        /*{
            int r,c;
            for(r=0;r<4;r++) {
                for(c=0;c<4;c++)
                    printf("%.15lg ", alpha[r][c]);
                printf("\n");
            }
        }*/
        assert(gluInvertMatrix(alpha, inv) == 1);
        /*{
            int r,c;
            for(r=0;r<4;r++) {
                for(c=0;c<4;c++)
                    printf("%.15lg ", inv[r][c]);
                printf("\n");
            }
        }*/
        /*for(r=0;r<4;r++) {
            printf("dp[%2d] = %.15lg\n", r, dp[r]);
        }*/

        dxu = inv[0][0] * dp[0] + inv[0][1] * dp[1] + inv[0][2] * dp[2] + inv[0][3] * dp[3];
        dyu = inv[1][0] * dp[0] + inv[1][1] * dp[1] + inv[1][2] * dp[2] + inv[1][3] * dp[3];
        dzu = inv[2][0] * dp[0] + inv[2][1] * dp[1] + inv[2][2] * dp[2] + inv[2][3] * dp[3];
        dbu = inv[3][0] * dp[0] + inv[3][1] * dp[1] + inv[3][2] * dp[2] + inv[3][3] * dp[3];

        /*{
            printf("dxu = %.15lg\n", dxu);
            printf("dyu = %.15lg\n", dyu);
            printf("dzu = %.15lg\n", dzu);
            printf("dbu = %.15lg\n", dbu);
        }
        exit(-2);*/

        xu += dxu;
        yu += dyu;
        zu += dzu;
        bu += dbu;
    }

    pos->x = xu;
    pos->y = yu;
    pos->z = zu;
    pos->b = bu;
}

static void dump_user_pos(struct pos *pos)
{
    printf("pos = %.15g / %.15g / %.15g | %.15g\n", pos->x, pos->y, pos->z, pos->b);
}

static void find_solution(struct fix *fix, struct pvt_info *info)
{
    int i;

    for(i = 0; i < 4; i++) {
        compute_sat_pos(info->gps_time, &info->eph[i], &fix->user, &fix->sat[i]);
        //dump_sat_pos(&fix->sat[i]);
    }
    compute_user_pos(&fix->user, &fix->sat[0], &fix->pseudo);
    //dump_user_pos(&fix->user);
}

static double compute_L(double Lc)
{
    int i;
    double L = Lc;

    for(i = 0; i < 10; ++i) {
        L = Lc + ep * sin(2 * L);
        //fprintf(stderr, "L = %.15g / %.15g\n", L, Lc);
    }

    return L;
}

static print_solution(struct position *position)
{
    struct coordinate coordinate ;

    compute_coordinate(position, &coordinate);

    printf("Latitude / Longitude / altitude = %.15g %.15g %.15g\n", coordinate.latitude, coordinate.longitude, coordinate.altitude);
}

void compute_coordinate(struct position *position, struct coordinate *coordinate)
{
    double Lc, l , L, h;
    double xu = position->x;
    double yu = position->y;
    double zu = position->z;

    Lc = atan(zu / sqrt(xu * xu + yu * yu));
    l = atan(yu / xu);
    L = compute_L(Lc);
    h = sqrt(xu*xu+yu*yu+zu*zu) - ae * (1 - ep * sin(L) *sin(L));

    coordinate->latitude = L * 180 / gps_pi;
    coordinate->longitude = l * 180 / gps_pi;
    coordinate->altitude = h;
}

void compute_fix(struct pvt_info *info, struct position *position)
{
    static fix_nb = 0;
    struct fix fix;
    int i;

    memset(&fix, 0, sizeof(fix));
    find_pseudo(&fix.pseudo, info);
    for (i = 0; i < 3; ++i)
        find_solution(&fix, info);
    position->x = fix.user.x;
    position->y = fix.user.y;
    position->z = fix.user.z;
    //print_solution(position);
}
